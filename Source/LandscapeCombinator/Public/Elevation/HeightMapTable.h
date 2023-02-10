#pragma once

#include "SlateTable.h"
#include "CoreMinimal.h"
#include "HMInterface.h"

class HeightMapTable : public SlateTable
{
public:
	HeightMapTable();
	
	TArray<FName> Landscapes;
	TMap<FString, HMInterface*> Interfaces;
	
	FString HeightMapListProjectFileV0;
	FString HeightMapListPluginFileV1;
	FString HeightMapListProjectFileV1;

	struct HeightMapDetailsV0 {
		FString LandscapeLabel;
		FString KindText;
		FString Descr;
		int Precision;

		friend FArchive& operator<<(FArchive& Ar, HeightMapDetailsV0& Details) {
			Ar << Details.LandscapeLabel;
			Ar << Details.KindText;
			Ar << Details.Descr;
			Ar << Details.Precision;
			return Ar;
		}
	};

	struct HeightMapDetailsV1 {
		FString LandscapeLabel;
		FString KindText;
		FString Descr;
		int32 Precision;
		bool Reproject;

		friend FArchive& operator<<(FArchive& Ar, HeightMapDetailsV1& Details) {
			Ar << Details.LandscapeLabel;
			Ar << Details.KindText;
			Ar << Details.Descr;
			Ar << Details.Precision;
			Ar << Details.Reproject;
			return Ar;
		}
	};
	
	HMInterface* InterfaceFromKind(FString LandscapeLabel0, const FText& KindText0, FString Descr0, int Precision0);
	FText DescriptionFromKind(const FText& KindText0);

	bool KindSupportsReprojection(const FText& KindText0);
	void AddHeightMapRow(FString LandscapeLabel, const FText& KindText, FString Descr, int Precision, bool bReproject, bool bSave);

	void Save() override;
	void LoadFrom(FString FilePath);
	void Load() override;
	TSharedRef<SWidget> Header() override;
	TSharedRef<SWidget> Footer() override;

	virtual void OnRemove(TSharedRef<SHorizontalBox> Line) override;
};