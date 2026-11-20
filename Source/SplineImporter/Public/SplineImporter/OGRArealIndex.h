// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class OGRGeometry;
class OGRPolygon;
class OGRLinearRing;

class SPLINEIMPORTER_API FOGRArealIndex
{
public:
	static TSharedRef<FOGRArealIndex> Create(const OGRGeometry* Geometry);

	bool Contains(double X, double Y) const;

	bool ContainsLonLat(double Lon, double Lat) const;

	bool IsEmpty() const { return Edges.IsEmpty(); }

private:
	FOGRArealIndex() = default;

	struct FEdge
	{
		double X1;
		double Y1;
		double Y2;
		double DXDY;
		int32 PolygonId;
	};

	void AddGeometry(const OGRGeometry* Geometry);
	void AddPolygon(const OGRPolygon* Polygon);
	void AddRing(const OGRLinearRing* Ring, int32 PolygonId);
	void AddEdge(double XA, double YA, double XB, double YB, int32 PolygonId);
	void BuildBuckets();
	int32 BucketOf(double Y) const;

	TArray<FEdge> Edges;
	TArray<int32> BucketStart;
	TArray<int32> BucketEdges;
	int32 NumPolygons = 0;
	int32 NumBuckets = 0;
	double InvBucketHeight = 0.0;
	double MinX = TNumericLimits<double>::Max();
	double MaxX = TNumericLimits<double>::Lowest();
	double MinY = TNumericLimits<double>::Max();
	double MaxY = TNumericLimits<double>::Lowest();
};
