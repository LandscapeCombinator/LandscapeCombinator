#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <unordered_set>
#include <sstream>
#include "SkeletonBuilder.h"
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>


static bool EqualEpsilon(double d1, double d2)
{
    return fabs(d1 - d2) < 5E-6;
    // return fabs(d1 - d2) < 1;
}

static bool ContainsEpsilon(std::vector<Vector2d> list, const Vector2d p)
{
    return std::any_of(list.begin(), list.end(), [&p](Vector2d l) { return EqualEpsilon(l.X, p.X) && EqualEpsilon(l.Y, p.Y); });
}

static std::vector<Vector2d> GetFacePoints(Skeleton sk)
{
    std::vector<Vector2d> ret = std::vector<Vector2d>();

    for (auto edgeOutput : *sk.Edges)
    {
        auto points = edgeOutput->Polygon;
        for (auto vector2d : *points)
        {
            if (!ContainsEpsilon(ret, *vector2d))
                ret.push_back(*vector2d);
        }
    }
    return ret;
}

static bool AssertExpectedPoints(std::vector<Vector2d> expectedList, std::vector<Vector2d> givenList)
{
    std::stringstream sb;
    for (const Vector2d& expected : expectedList)
    {
        if (!ContainsEpsilon(givenList, expected))
            sb << std::string("Can't find expected point ("+ std::to_string(expected.X) +", "+ std::to_string(expected.Y) +") in given list\n");
    }

    for (const Vector2d& given : givenList)
    {
        if (!ContainsEpsilon(expectedList, given))
            sb << std::string("Can't find given point (" + std::to_string(given.X) + ", " + std::to_string(given.Y) + ") in expected list\n");
    }

    if (sb.tellp() > 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void BuildSkeleton()
{
    std::vector<Vector2d> polygon = {
        Vector2d(0.0, 0.0),
        Vector2d(7.087653026630875, -0.0572739636795121),
        Vector2d(7.035244566479503, -6.5428208800475005),
        Vector2d(-0.052408459722688594, -6.485546915224834)
    };

    std::vector<Vector2d> hole = {
        Vector2d(1.4849939588531493, -1.5250224044562133),
        Vector2d(1.4341762422598874, -5.1814705083480606),
        Vector2d(5.747532319228888, -5.241418004618678),
        Vector2d(5.798350035536362, -1.5849699030131408)
    };

    std::vector<std::vector<Vector2d>> holes = { hole };

    std::vector<Vector2d> expected = {
        Vector2d(6.3821371859978875, -5.893911100019249),
        Vector2d(0.7651208111455217, -5.8321836510415475),
        Vector2d(0.6898242249025952, -5.755213752675646),
        Vector2d(6.389576876981116, -5.886633146615758),
        Vector2d(6.443747494495353, -0.9572661447277495),
        Vector2d(6.310953658294117, -0.8215212379272131),
        Vector2d(0.7481994722534444, -0.7603900949775717),
        Vector2d(0.7446762937827887, -0.7638366801629576)
    };

    expected.insert(expected.end(), polygon.begin(), polygon.end());
    expected.insert(expected.end(), hole.begin(), hole.end());

    std::cout << "inital polygon: " << "\n";
    for (auto p : polygon)
    {
        std::cout << p.ToString() << "\n";
    }

    std::cout << "build: " << "\n";
    auto sk = SkeletonBuilder::Build(polygon, holes);

    std::cout << "output: " << "\n";
    for (auto e : *sk.Distances)
    {
        std::cout << e.first.ToString() << "\n";
    }
    AssertExpectedPoints(expected, GetFacePoints(sk));
}

int main()
{	
  BuildSkeleton();
}