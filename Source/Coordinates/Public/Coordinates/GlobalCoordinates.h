// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "GDALInterface/GDALInterface.h"

#include "Landscape.h"

#include "GlobalCoordinates.generated.h"

UENUM(BlueprintType)
enum class EBodyModel : uint8
{
	Flat,
	Sphere
};

UCLASS(BlueprintType)
class COORDINATES_API UGlobalCoordinates : public UActorComponent
{
	GENERATED_BODY()

public:

	/* Coordinate System */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (EditCondition = "BodyModel == EBodyModel::Flat", EditConditionHides, DisplayPriority = "1")
	)
	FString CRS = "EPSG:4326";

	/* Number of Unreal Engine centimeters per longitude coordinate system unit */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (EditCondition = "BodyModel == EBodyModel::Flat", EditConditionHides, DisplayPriority = "2")
	)
	double CmPerLongUnit = 11111111;
	
	/* Number of Unreal Engine centimeters per latitude coordinate system unit */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (EditCondition = "BodyModel == EBodyModel::Flat", EditConditionHides, DisplayPriority = "3")
	)
	double CmPerLatUnit = -11111111;
	
	/* Longitude of the World Origin (in the given Coordinate System) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "5")
	)
	double WorldOriginLong = 0;
	
	/* Latitude of the World Origin (in the given Coordinate System) */
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "6")
	)
	double WorldOriginLat = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (DisplayPriority = "0")
	)
	EBodyModel BodyModel = EBodyModel::Flat;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "GlobalCoordinates",
		meta = (EditCondition = "BodyModel == EBodyModel::Sphere", EditConditionHides, DisplayPriority = "8")
	)
	double BodyRadiusCm = 637100000;

	/* World location of the surface point at (WorldOriginLat, WorldOriginLong). It is kept in sync with the location of
	 * the Level Coordinates actor, so that several bodies can sit at different places of the same level. The default
	 * (world origin) gives the same results as before. Sphere model only. */
	UPROPERTY()
	FVector BodyLocation = FVector::ZeroVector;

	UPROPERTY()
	FQuat BodyRotation = FQuat::Identity;

	/* World location of the center of the sphere */
	FVector SphereCenter() const { return BodyLocation + FVector(0, 0, -BodyRadiusCm); }

	/* BodyLocation as a sum of two float vectors (High + Low), which keeps double precision for material parameters
	 * (floats are only precise to about 7 digits). In the material, subtract both from the absolute world position (with LWC nodes). */
	void GetBodyLocationHighLow(FLinearColor& OutHigh, FLinearColor& OutLow) const
	{
		const float HX = (float) BodyLocation.X, HY = (float) BodyLocation.Y, HZ = (float) BodyLocation.Z;
		OutHigh = FLinearColor(HX, HY, HZ, 0.f);
		OutLow = FLinearColor((float) (BodyLocation.X - (double) HX), (float) (BodyLocation.Y - (double) HY), (float) (BodyLocation.Z - (double) HZ), 0.f);
	}

	/* This function is slow. If you need to call it many times, make a single OGRCoordinateTransformation
	 * with `GetCRSTransformer`and use this instead. */
	bool GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FString FromCRS, FVector2D &XY);
	
	OGRCoordinateTransformation *GetCRSTransformer(FString FromCRS);
	void GetCRSCoordinatesFromUnrealLocation(FVector2D Location, FVector2D& OutCoordinates);
	void GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, FVector2D &XY);
	bool GetUnrealCoordinatesFromCRS(double Longitude, double Latitude, OGRCoordinateTransformation *CoordinateTransformation, FVector2D &XY);
	bool GetCRSCoordinatesFromUnrealLocation(FVector2D Location, FString ToCRS, FVector2D& OutCoordinates);
	bool GetCRSCoordinatesFromUnrealLocation(FVector2D Location, OGRCoordinateTransformation *CoordinateTransformation, FVector2D& OutCoordinates);
	void GetCRSCoordinatesFromUnrealLocations(FVector4d Locations, FVector4d& OutCoordinates);
	bool GetCRSCoordinatesFromUnrealLocations(FVector4d Locations, FString ToCRS, FVector4d& OutCoordinates);
	bool GetCRSCoordinatesFromFBox(FBox Box, FString ToCRS, FVector4d& OutCoordinates, bool bShowDialog = true);
	bool GetCRSCoordinatesFromOriginExtent(FVector Origin, FVector Extent, FString ToCRS, FVector4d& OutCoordinates, bool bShowDialog = true);

	bool GetSphereFootprint(const FBox& Box, double& OutLatMin, double& OutLatMax, double& OutLonMin, double& OutLonMax) const;

	FVector ToWorld(double Long, double Lat, double AltCm = 0) const;
	void FromWorld(const FVector& World, double& OutLong, double& OutLat, double& OutAltCm) const;

	bool GetLonLatAltFromWorld(const FVector& World, double& OutLong, double& OutLat, double& OutAltCm);

	FVector Up(const FVector& World) const;
	FQuat Alignment(const FVector& World) const;

	void SetSphereMappingParameters(class UMaterialInstanceDynamic* MID, const FVector2D& BoundsMin, const FVector2D& BoundsMax) const;

	static bool IsSphere(const UGlobalCoordinates* G) { return G && G->IsSphere(); }
	static FVector Up(const UGlobalCoordinates* G, const FVector& World) { return G ? G->Up(World) : FVector::UpVector; }
	static FQuat Alignment(const UGlobalCoordinates* G, const FVector& World) { return G ? G->Alignment(World) : FQuat::Identity; }
	static double GetSphereRadiusCm(const UGlobalCoordinates* G) { return (G && G->IsSphere()) ? G->BodyRadiusCm : 0.0; }
	bool IsSphere() const { return BodyModel == EBodyModel::Sphere; }

	static void ComputeSphereDecalPlacement(const UGlobalCoordinates* G, const FVector2D& BoundsMin, const FVector2D& BoundsMax, FVector& OutCenter, FQuat& OutRotation, FVector& OutSize);

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static FVector4d GetCoordinatesFromSize(double Longitude, double Latitude, double Width, double Height, double SphereRadiusCm = 0);
};
