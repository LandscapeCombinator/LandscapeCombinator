// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SplineImporter/GDALImporter.h"
#include "GDALInterface/GDALInterface.h"

#include "OGRGeometry.generated.h"

class UDecalComponent;
class UMaterialInterface;

UCLASS(BlueprintType)
class SPLINEIMPORTER_API AOGRGeometry : public AGDALImporter
{
	GENERATED_BODY()

public:

	AOGRGeometry();

	/* Tag to apply to the current actor when importing the area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "5")
	)
	FName AreaTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "6")
	)
	bool bClearGeometryBeforeImporting = true;

	/* Show a colored decal overlay of Geometry on the landscape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter|Preview",
		meta = (DisplayPriority = "10")
	)
	bool bShowGeometryPreview = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter|Preview",
		meta = (DisplayPriority = "11")
	)
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter|Preview", meta = (DisplayPriority = "12"))
	int32 PreviewResolution = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter|Preview", meta = (DisplayPriority = "13"))
	FLinearColor PreviewColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.1f);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GDALImporter|Preview",
		meta = (DisplayPriority = "13", ShowOnlyInnerProperties)
	)
	TObjectPtr<UDecalComponent> PreviewDecal = nullptr;

	/* Simple cache, the serialized data is saved in a UPROPERTY */
	OGRGeometry *Geometry = nullptr;

	bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GDALImporter")
	void UpdateGeometryPreview();

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

	virtual void PostLoad() override;
	virtual void PostInitializeComponents() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	OGRGeometry* CloneGeometry() const;

protected:
	virtual void SetOverpassShortQuery() override;

	void HideGeometryPreview();

	UPROPERTY()
	TArray<uint8> SerializedGeometry;

	void SetGeometry(OGRGeometry* NewGeometry);
	void RebuildGeometryFromSerialized();

    mutable FCriticalSection GeometryLock;
};
