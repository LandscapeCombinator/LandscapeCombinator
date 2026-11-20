// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/OGRArealIndex.h"

#include "GDALInterface/GDALInterface.h"

TSharedRef<FOGRArealIndex> FOGRArealIndex::Create(const OGRGeometry* Geometry)
{
	TSharedRef<FOGRArealIndex> Index = MakeShareable(new FOGRArealIndex());
	Index->AddGeometry(Geometry);
	Index->BuildBuckets();
	return Index;
}

void FOGRArealIndex::AddGeometry(const OGRGeometry* Geometry)
{
	if (!Geometry || Geometry->IsEmpty()) return;

	switch (wkbFlatten(Geometry->getGeometryType()))
	{
	case wkbPolygon:
		AddPolygon(Geometry->toPolygon());
		break;
	case wkbCurvePolygon:
	case wkbMultiSurface:
	{
		OGRGeometry* Linear = Geometry->getLinearGeometry();
		if (Linear)
		{
			AddGeometry(Linear);
			OGRGeometryFactory::destroyGeometry(Linear);
		}
		break;
	}
	case wkbMultiPolygon:
	case wkbGeometryCollection:
	{
		const OGRGeometryCollection* Collection = Geometry->toGeometryCollection();
		for (int32 i = 0, n = Collection->getNumGeometries(); i < n; ++i) AddGeometry(Collection->getGeometryRef(i));
		break;
	}
	default:
		break;
	}
}

void FOGRArealIndex::AddPolygon(const OGRPolygon* Polygon)
{
	if (!Polygon || Polygon->IsEmpty()) return;

	const int32 PolygonId = NumPolygons++;
	if (const auto* Exterior = Polygon->getExteriorRing()) AddRing(Exterior, PolygonId);
	for (int32 i = 0, n = Polygon->getNumInteriorRings(); i < n; ++i)
	{
		if (const auto* Hole = Polygon->getInteriorRing(i)) AddRing(Hole, PolygonId);
	}
}

void FOGRArealIndex::AddRing(const OGRLinearRing* Ring, int32 PolygonId)
{
	const int32 Num = Ring->getNumPoints();
	if (Num < 3) return;

	for (int32 i = 0; i + 1 < Num; ++i)
	{
		AddEdge(Ring->getX(i), Ring->getY(i), Ring->getX(i + 1), Ring->getY(i + 1), PolygonId);
	}
	if (Ring->getX(0) != Ring->getX(Num - 1) || Ring->getY(0) != Ring->getY(Num - 1))
	{
		AddEdge(Ring->getX(Num - 1), Ring->getY(Num - 1), Ring->getX(0), Ring->getY(0), PolygonId);
	}
}

void FOGRArealIndex::AddEdge(double XA, double YA, double XB, double YB, int32 PolygonId)
{
	if (YA == YB) return;
	if (!FMath::IsFinite(XA) || !FMath::IsFinite(YA) || !FMath::IsFinite(XB) || !FMath::IsFinite(YB)) return;

	if (YA > YB)
	{
		Swap(XA, XB);
		Swap(YA, YB);
	}

	Edges.Add({ XA, YA, YB, (XB - XA) / (YB - YA), PolygonId });

	MinX = FMath::Min3(MinX, XA, XB);
	MaxX = FMath::Max3(MaxX, XA, XB);
	MinY = FMath::Min(MinY, YA);
	MaxY = FMath::Max(MaxY, YB);
}

int32 FOGRArealIndex::BucketOf(double Y) const
{
	return FMath::Clamp(FMath::FloorToInt((Y - MinY) * InvBucketHeight), 0, NumBuckets - 1);
}

void FOGRArealIndex::BuildBuckets()
{
	if (Edges.IsEmpty()) return;

	NumBuckets = FMath::Clamp(Edges.Num() / 8, 1, 1 << 20);
	const double Height = MaxY - MinY;
	InvBucketHeight = Height > 0.0 ? double(NumBuckets) / Height : 0.0;

	BucketStart.SetNumZeroed(NumBuckets + 1);
	for (const FEdge& Edge : Edges)
	{
		for (int32 B = BucketOf(Edge.Y1), Last = BucketOf(Edge.Y2); B <= Last; ++B) ++BucketStart[B + 1];
	}
	for (int32 B = 0; B < NumBuckets; ++B) BucketStart[B + 1] += BucketStart[B];

	BucketEdges.SetNumUninitialized(BucketStart[NumBuckets]);
	TArray<int32> Cursor(BucketStart.GetData(), NumBuckets);
	for (int32 i = 0; i < Edges.Num(); ++i)
	{
		for (int32 B = BucketOf(Edges[i].Y1), Last = BucketOf(Edges[i].Y2); B <= Last; ++B) BucketEdges[Cursor[B]++] = i;
	}
}

bool FOGRArealIndex::Contains(double X, double Y) const
{
	if (Edges.IsEmpty() || Y < MinY || Y > MaxY || X < MinX || X > MaxX) return false;

	int32 CurrentPolygon = -1;
	bool bOdd = false;
	const int32 Bucket = BucketOf(Y);
	for (int32 i = BucketStart[Bucket], End = BucketStart[Bucket + 1]; i < End; ++i)
	{
		const FEdge& Edge = Edges[BucketEdges[i]];
		if (Edge.PolygonId != CurrentPolygon)
		{
			if (bOdd) return true;
			CurrentPolygon = Edge.PolygonId;
			bOdd = false;
		}
		if (Y >= Edge.Y1 && Y < Edge.Y2 && X < Edge.X1 + (Y - Edge.Y1) * Edge.DXDY) bOdd = !bOdd;
	}
	return bOdd;
}

bool FOGRArealIndex::ContainsLonLat(double Lon, double Lat) const
{
	if (Contains(Lon, Lat)) return true;
	if (MaxX > 180.0 && Contains(Lon + 360.0, Lat)) return true;
	if (MinX < -180.0 && Contains(Lon - 360.0, Lat)) return true;
	return false;
}
