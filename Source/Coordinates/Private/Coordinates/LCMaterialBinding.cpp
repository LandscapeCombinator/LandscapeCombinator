// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/LCMaterialBinding.h"
#include "Coordinates/LevelCoordinates.h"
#include "Coordinates/GlobalCoordinates.h"
#include "Coordinates/LogCoordinates.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialLayersFunctions.h"
#include "Engine/Texture.h"
#include "Containers/Ticker.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

const FName ULCMaterialBinding::TextureParameterName = TEXT("Texture");
const FName ULCMaterialBinding::BoundsMinParameterName = TEXT("BoundsMin");
const FName ULCMaterialBinding::BoundsMaxParameterName = TEXT("BoundsMax");
const FName ULCMaterialBinding::MaskParameterName = TEXT("Mask");
const FName ULCMaterialBinding::MaskBoundsMinParameterName = TEXT("MaskBoundsMin");
const FName ULCMaterialBinding::MaskBoundsMaxParameterName = TEXT("MaskBoundsMax");
const FName ULCMaterialBinding::UseMaskParameterName = TEXT("UseMask");
const FName ULCMaterialBinding::BodyParameterName = TEXT("Body");
const FName ULCMaterialBinding::BodyLocationHighParameterName = TEXT("BodyLocationHigh");
const FName ULCMaterialBinding::BodyLocationLowParameterName = TEXT("BodyLocationLow");

namespace
{
	UMaterialInterface* StripOwnInstance(UMaterialInterface* Material, const UObject* Owner)
	{
		UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
		if (Instance && Instance->GetOuter() == Owner) return Instance->Parent;
		return Material;
	}
}

bool ULCMaterialBinding::BindTexture(
	AActor* Target, const FString& SourceKey, FName LayerName,
	UTexture* Texture, const FVector2D& BoundsMin, const FVector2D& BoundsMax, bool bMask)
{
	TWeakObjectPtr<AActor> WeakTarget(Target);
	TWeakObjectPtr<UTexture> WeakTexture(Texture);

	bool bResult = false;
	Concurrency::RunOnGameThreadAndWait([&]()
	{
		if (!WeakTarget.IsValid() || !WeakTexture.IsValid() || LayerName.IsNone()) return false;
		AActor* T = WeakTarget.Get();

		ULCMaterialBinding* Binding = T->FindComponentByClass<ULCMaterialBinding>();
		if (!Binding)
		{
			Binding = NewObject<ULCMaterialBinding>(T, NAME_None, RF_Transactional);
			T->AddInstanceComponent(Binding);
			Binding->RegisterComponent();
		}

		Binding->Modify();
		FLCLayerTextureEntry* Entry = Binding->Entries.FindByPredicate([&](const FLCLayerTextureEntry& E) { return E.SourceKey == SourceKey; });
		if (!Entry) Entry = &Binding->Entries.AddDefaulted_GetRef();
		Entry->SourceKey = SourceKey;
		Entry->LayerName = LayerName;
		Entry->LayerIndex = INDEX_NONE;
		Entry->Texture = WeakTexture.Get();
		Entry->bMask = bMask;
		Entry->BoundsMin = BoundsMin;
		Entry->BoundsMax = BoundsMax;

		Binding->Rebuild();
		T->MarkPackageDirty();
		bResult = true;
		return true;
	});
	return bResult;
}

void ULCMaterialBinding::RemoveSource(AActor* Target, const FString& SourceKey)
{
	TWeakObjectPtr<AActor> WeakTarget(Target);
	Concurrency::RunOnGameThreadAndWait([&]()
	{
		if (!WeakTarget.IsValid()) return false;
		ULCMaterialBinding* Binding = WeakTarget->FindComponentByClass<ULCMaterialBinding>();
		if (!Binding) return true;
		Binding->Modify();
		Binding->Entries.RemoveAll([&](const FLCLayerTextureEntry& E) { return E.SourceKey == SourceKey; });
		Binding->Rebuild();
		return true;
	});
}

void ULCMaterialBinding::RemoveSourcesWithPrefix(AActor* Target, const FString& KeyPrefix)
{
	TWeakObjectPtr<AActor> WeakTarget(Target);
	Concurrency::RunOnGameThreadAndWait([&]()
	{
		if (!WeakTarget.IsValid()) return false;
		ULCMaterialBinding* Binding = WeakTarget->FindComponentByClass<ULCMaterialBinding>();
		if (!Binding) return true;
		auto Matches = [&](const FLCLayerTextureEntry& E) { return E.SourceKey.StartsWith(KeyPrefix); };
		if (!Binding->Entries.ContainsByPredicate(Matches)) return true;
		Binding->Modify();
		Binding->Entries.RemoveAll(Matches);
		Binding->Rebuild();
		return true;
	});
}

void ULCMaterialBinding::OnRegister()
{
	Super::OnRegister();
	RequestRebuild();
}

void ULCMaterialBinding::RequestRebuild()
{
	if (bRebuildPending) return;
	bRebuildPending = true;

	// Deferred so that the landscape material is never swapped while components are registering
	TWeakObjectPtr<ULCMaterialBinding> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (ULCMaterialBinding* This = WeakThis.Get())
		{
			This->bRebuildPending = false;
			if (This->IsRegistered()) This->Rebuild();
		}
		return false;
	}));
}

void ULCMaterialBinding::FindSlots()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	if (ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Owner))
	{
		if (Slots.Num() == 0) Slots.AddDefaulted();
		FLCBoundSlot& Slot = Slots[0];
		Slot.ComponentName = NAME_None;
		UMaterialInterface* Base = StripOwnInstance(Landscape->LandscapeMaterial, Landscape);
		if (IsValid(Base)) Slot.BaseMaterial = Base;
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	Owner->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* Component : MeshComponents)
	{
		for (int32 Index = 0; Index < Component->GetNumMaterials(); Index++)
		{
			FLCBoundSlot* Slot = Slots.FindByPredicate([&](const FLCBoundSlot& S) { return S.ComponentName == Component->GetFName() && S.SlotIndex == Index; });
			if (!Slot)
			{
				Slot = &Slots.AddDefaulted_GetRef();
				Slot->ComponentName = Component->GetFName();
				Slot->SlotIndex = Index;
			}
			UMaterialInterface* Base = StripOwnInstance(Component->GetMaterial(Index), Component);
			if (IsValid(Base)) Slot->BaseMaterial = Base;
		}
	}
}

UMaterialInstance* ULCMaterialBinding::GetOrCreateInstance(const FLCBoundSlot& Slot)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(Slot.BaseMaterial)) return nullptr;

	if (ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Owner))
	{
		// Landscape components parent their own material instances to LandscapeMaterial, and the engine
		// rejects MIDs as parents, so landscapes get a MIC that is only edited in the editor.
#if WITH_EDITOR
		UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Landscape->LandscapeMaterial);
		if (!MIC || MIC->GetOuter() != Landscape || MIC->Parent != Slot.BaseMaterial)
		{
			// Public so that streaming proxies, which copy LandscapeMaterial, can reference it from their packages
			MIC = NewObject<UMaterialInstanceConstant>(Landscape, NAME_None, RF_Public | RF_Transactional);
			MIC->SetParentEditorOnly(Slot.BaseMaterial);
			Landscape->Modify();
			Landscape->LandscapeMaterial = MIC;
			FPropertyChangedEvent Event(FindFieldChecked<FProperty>(ALandscapeProxy::StaticClass(), TEXT("LandscapeMaterial")));
			Landscape->PostEditChangeProperty(Event);
		}
		return MIC;
#else
		return nullptr;
#endif
	}

	for (UMeshComponent* Component : TInlineComponentArray<UMeshComponent*>(Owner))
	{
		if (Component->GetFName() != Slot.ComponentName || Slot.SlotIndex >= Component->GetNumMaterials()) continue;
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(Slot.SlotIndex));
		if (!MID || MID->GetOuter() != Component || MID->Parent != Slot.BaseMaterial)
		{
			MID = UMaterialInstanceDynamic::Create(Slot.BaseMaterial, Component);
			Component->SetMaterial(Slot.SlotIndex, MID);
		}
		return MID;
	}
	return nullptr;
}

void ULCMaterialBinding::WriteParameters(UMaterialInstance* Instance)
{
	if (!IsValid(Instance)) return;
	AActor* Owner = GetOwner();

	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Instance);
#if WITH_EDITOR
	UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Instance);
#endif

	auto SetVector = [&](const FMaterialParameterInfo& Info, const FLinearColor& Value)
	{
		if (MID) MID->SetVectorParameterValueByInfo(Info, Value);
#if WITH_EDITOR
		else if (MIC) MIC->SetVectorParameterValueEditorOnly(Info, Value);
#endif
	};
	auto SetScalar = [&](const FMaterialParameterInfo& Info, float Value)
	{
		if (MID) MID->SetScalarParameterValueByInfo(Info, Value);
#if WITH_EDITOR
		else if (MIC) MIC->SetScalarParameterValueEditorOnly(Info, Value);
#endif
	};
	auto SetTexture = [&](const FMaterialParameterInfo& Info, UTexture* Value)
	{
		if (MID) MID->SetTextureParameterValueByInfo(Info, Value);
#if WITH_EDITOR
		else if (MIC) MIC->SetTextureParameterValueEditorOnly(Info, Value);
#endif
	};

	if (MID) MID->ClearParameterValues();
#if WITH_EDITOR
	else if (MIC) MIC->ClearParameterValuesEditorOnly();
#endif

	FMaterialLayersFunctions Layers;
	const bool bHasLayers = Instance->GetMaterialLayers(Layers);
	const int32 NumLayers = bHasLayers ? Layers.Layers.Num() : 0;
	const int32 NumBlends = bHasLayers ? Layers.Blends.Num() : 0;

	if (const UGlobalCoordinates* G = ALevelCoordinates::GetGlobalCoordinatesFor(Owner, false))
	{
		const FLinearColor Body = ALevelCoordinates::MakeBodyVector(G);
		FLinearColor LocationHigh, LocationLow;
		G->GetBodyLocationHighLow(LocationHigh, LocationLow);

		auto SetAll = [&](FName Name, const FLinearColor& Value)
		{
			SetVector(FMaterialParameterInfo(Name), Value);
			for (int32 i = 0; i < NumLayers; i++)
				SetVector(FMaterialParameterInfo(Name, EMaterialParameterAssociation::LayerParameter, i), Value);
			for (int32 i = 0; i < NumBlends; i++)
				SetVector(FMaterialParameterInfo(Name, EMaterialParameterAssociation::BlendParameter, i), Value);
		};
		SetAll(BodyParameterName, Body);
		SetAll(BodyLocationHighParameterName, LocationHigh);
		SetAll(BodyLocationLowParameterName, LocationLow);
	}

	for (FLCLayerTextureEntry& Entry : Entries)
	{
		if (!IsValid(Entry.Texture)) continue;

#if WITH_EDITORONLY_DATA
		const FString Wanted = Entry.LayerName.ToString();
		int32 Found = INDEX_NONE;
		for (int32 i = 0; i < Layers.EditorOnly.LayerNames.Num(); i++)
		{
			if (Layers.EditorOnly.LayerNames[i].ToString().Equals(Wanted, ESearchCase::IgnoreCase)) { Found = i; break; }
		}
		if (Found == INDEX_NONE && bHasLayers)
		{
			TArray<FString> Names;
			for (const FText& Name : Layers.EditorOnly.LayerNames) Names.Add(Name.ToString());
			LCReporter::ShowError(FText::Format(
				LOCTEXT("ULCMaterialBinding::LayerNotFound", "Layer '{0}' was not found in the material layers of {1}. Available layers: {2}"),
				FText::FromString(Wanted), FText::FromString(Owner->GetActorNameOrLabel()), FText::FromString(FString::Join(Names, TEXT(", ")))
			));
			continue;
		}
		if (Found != INDEX_NONE) Entry.LayerIndex = Found;
#endif
		const int32 LayerIndex = Entry.LayerIndex;
		if (LayerIndex == INDEX_NONE) continue;

		const FLinearColor Min((float) Entry.BoundsMin.X, (float) Entry.BoundsMin.Y, 0.f, 0.f);
		const FLinearColor Max((float) Entry.BoundsMax.X, (float) Entry.BoundsMax.Y, 0.f, 0.f);

		if (Entry.bMask)
		{
			// The blend of layer i is at index i - 1, the base layer has no blend
			if (LayerIndex == 0)
			{
				LCReporter::ShowError(FText::Format(
					LOCTEXT("ULCMaterialBinding::MaskOnBaseLayer", "Layer '{0}' is the base layer of {1}, which has no blend for the mask. Please use another layer."),
					FText::FromName(Entry.LayerName), FText::FromString(Owner->GetActorNameOrLabel())
				));
				continue;
			}
			const FMaterialParameterInfo Info(MaskParameterName, EMaterialParameterAssociation::BlendParameter, LayerIndex - 1);
			SetTexture(Info, Entry.Texture);
			SetVector(FMaterialParameterInfo(MaskBoundsMinParameterName, EMaterialParameterAssociation::BlendParameter, LayerIndex - 1), Min);
			SetVector(FMaterialParameterInfo(MaskBoundsMaxParameterName, EMaterialParameterAssociation::BlendParameter, LayerIndex - 1), Max);
			SetScalar(FMaterialParameterInfo(UseMaskParameterName, EMaterialParameterAssociation::BlendParameter, LayerIndex - 1), 1.f);
			continue;
		}

		auto Write = [&](EMaterialParameterAssociation Association, int32 Index)
		{
			SetTexture(FMaterialParameterInfo(TextureParameterName, Association, Index), Entry.Texture);
			SetVector(FMaterialParameterInfo(BoundsMinParameterName, Association, Index), Min);
			SetVector(FMaterialParameterInfo(BoundsMaxParameterName, Association, Index), Max);
		};
		Write(EMaterialParameterAssociation::LayerParameter, LayerIndex);
		if (LayerIndex > 0) Write(EMaterialParameterAssociation::BlendParameter, LayerIndex - 1);
	}
}

void ULCMaterialBinding::Rebuild()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Owner);

	// Landscape MICs can only be edited in the editor; in game worlds they keep their saved parameters
	if (Landscape)
	{
#if WITH_EDITOR
		if (!IsValid(GetWorld()) || GetWorld()->IsGameWorld()) return;
#else
		return;
#endif
	}

	FindSlots();
	for (const FLCBoundSlot& Slot : Slots)
	{
		UMaterialInstance* Instance = GetOrCreateInstance(Slot);
		if (!Instance)
		{
			UE_LOG(LogCoordinates, Warning, TEXT("ULCMaterialBinding: could not create a material instance for %s"), *Owner->GetName());
			continue;
		}
		WriteParameters(Instance);

#if WITH_EDITOR
		if (Landscape) Instance->PostEditChange();
#endif
	}

#if WITH_EDITOR
	if (Landscape)
	{
		if (ULandscapeInfo* Info = Landscape->GetLandscapeInfo()) Info->UpdateAllComponentMaterialInstances();
		else Landscape->UpdateAllComponentMaterialInstances();
	}
#endif
}

#undef LOCTEXT_NAMESPACE
