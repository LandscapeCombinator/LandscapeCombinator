// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/BodyNavigation.h"

void FBodyNavigation::Apply(const UGlobalCoordinates& G, const FVector* LastLocation, const FRotator* LastRotation, FVector& Location, FRotator& Rotation)
{
	const FRotator CurrentRotation = Rotation;
	FQuat Q = CurrentRotation.Quaternion();

	if (LastLocation && LastRotation)
	{
		const FVector Up0 = G.Up(*LastLocation);
		const double dYaw = FRotator::NormalizeAxis(CurrentRotation.Yaw - LastRotation->Yaw);
		const double dPitch = FRotator::NormalizeAxis(CurrentRotation.Pitch - LastRotation->Pitch);
		if (FMath::Abs(dYaw) < 60 && FMath::Abs(dPitch) < 60)
		{
			Q = FQuat(Up0, FMath::DegreesToRadians(dYaw)) * LastRotation->Quaternion();
			const double Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FVector::DotProduct(Q.GetForwardVector(), Up0), -1.0, 1.0)));
			const double NewPitch = FMath::Clamp(Pitch + dPitch, -89.9, 89.9);
			Q = FQuat(Q.GetRightVector(), FMath::DegreesToRadians(-(NewPitch - Pitch))) * Q;
		}

		const FVector Delta = Location - *LastLocation;
		const double DeltaSq = Delta.SizeSquared();
		if (DeltaSq > 1.0 && DeltaSq < 1e14)
		{
			const FVector PrevCenter = *LastLocation - G.SphereCenter(); // from the center of the body
			const double Vertical = FVector::DotProduct(Delta, Up0);
			const FVector Tangent = Delta - Up0 * Vertical;
			const FVector NewDir = (PrevCenter + Tangent).GetSafeNormal();
			const FVector NewCenter = NewDir * (PrevCenter.Size() + Vertical);
			Location = G.SphereCenter() + NewCenter;
			Q = FQuat::FindBetweenNormals(Up0, NewDir) * Q;
		}
	}

	const FVector Up = G.Up(Location);
	const FVector Forward = Q.GetForwardVector();
	if (FMath::Abs(FVector::DotProduct(Forward, Up)) < 0.9999999)
	{
		Rotation = FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
	}
	else
	{
		Rotation = Q.Rotator();
	}
}
