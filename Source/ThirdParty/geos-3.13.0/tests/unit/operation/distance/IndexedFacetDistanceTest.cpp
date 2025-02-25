//
// Test Suite for geos::operation::distance::DistanceOp class.

#define _USE_MATH_DEFINES

// std
#include <memory>
#include <string>
#include <vector>
#include <cmath>

// tut
#include <tut/tut.hpp>
#include <tut/tut_macros.hpp>
#include <utility.h>
// geos
#include <geos/profiler.h>
#include <geos/constants.h>
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LineSegment.h>
#include <geos/geom/LineString.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/io/WKTReader.h>
#include <geos/io/WKTWriter.h>
#include <geos/operation/distance/DistanceOp.h>
#include <geos/operation/distance/IndexedFacetDistance.h>
#include <geos/util/GEOSException.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using geos::operation::distance::IndexedFacetDistance;
using geos::geom::Coordinate;

namespace tut {
//
// Test Group
//

// Common data used by tests
struct test_facetdistanceop_data {

    typedef std::unique_ptr<geos::geom::Geometry> GeomPtr;
    typedef std::unique_ptr<geos::geom::CoordinateSequence> CSPtr;

    geos::io::WKTWriter _wktwriter;
    geos::geom::PrecisionModel _pm;
    geos::geom::GeometryFactory::Ptr _factory;
    geos::io::WKTReader _wktreader;


    test_facetdistanceop_data()
        : _wktwriter()
        , _pm(geos::geom::PrecisionModel::FLOATING)
        , _factory(geos::geom::GeometryFactory::create(&_pm, 0))
        , _wktreader(_factory.get())
    {}

    void
    checkDistanceNearestPoints(std::string wkt1, std::string wkt2, double distanceExpected,
                               const geos::geom::Coordinate& p1, 
                               const geos::geom::Coordinate& p2)
    {
        using geos::operation::distance::IndexedFacetDistance;
        GeomPtr g1(_wktreader.read(wkt1));
        GeomPtr g2(_wktreader.read(wkt2));
        auto pts = IndexedFacetDistance::nearestPoints(g1.get(), g2.get());
        const Coordinate p1Actual = (*pts)[0];
        const Coordinate p2Actual = (*pts)[1];
        double distResult = (*pts)[0].distance((*pts)[1]);

        ensure_equals("Distance", distResult, distanceExpected, 1e-4);
        ensure_equals_xy(p1Actual, p1, 1e-08);
        ensure_equals_xy(p2Actual, p2, 1e-08);
    }

    void
    checkWithinDistance(std::string wkt1, std::string wkt2, double distance,
                               bool expected)
    {
        using geos::operation::distance::IndexedFacetDistance;
        std::unique_ptr<geos::geom::Geometry> g1(_wktreader.read(wkt1));
        std::unique_ptr<geos::geom::Geometry> g2(_wktreader.read(wkt2));

        IndexedFacetDistance ifd(g1.get());
        bool result = ifd.isWithinDistance(g2.get(), distance);
        ensure_equals(result, expected);
    }

    int
    angle2sincircle(double theta_deg, double radius, double amplitude, double* x, double* y)
    {
        // 𝑟=𝑅+𝑎sin(𝑛𝜃)
        int n = 16;
        double theta = theta_deg * M_PI / 180.0;
        double a = radius * amplitude;
        double r = radius + a * std::sin(n*theta);
        if (x) *x = r * std::cos(theta);
        if (y) *y = r * std::sin(theta);
        return 1;
    }


    std::unique_ptr<geos::geom::LineString>
    makeSinCircle(std::size_t nvertices, double radius, double amplitude)
    {
        auto cs = geos::detail::make_unique<geos::geom::CoordinateSequence>();
        std::vector<geos::geom::Coordinate> coords;
        for (std::size_t i = 0; i < nvertices; i++) {
            geos::geom::Coordinate c;
            double angle = (double)i*360.0/(double)nvertices;
            angle2sincircle(angle, radius, amplitude, &c.x, &c.y);
            cs->add(c);
        }

        auto ls = _factory->createLineString(std::move(cs));
        return ls;
    }

};

typedef test_group<test_facetdistanceop_data> group;
typedef group::object object;

group test_facetdistanceop_group("geos::operation::distance::IndexedFacetDistance");


//
// Test Cases
//
template<>
template<>
void object::test<1>
()
{
    using geos::operation::distance::IndexedFacetDistance;

    std::string wkt0("POINT(0 0)");
    std::string wkt1("POINT(10 0)");
    GeomPtr g0(_wktreader.read(wkt0));
    GeomPtr g1(_wktreader.read(wkt1));
    double d = IndexedFacetDistance::distance(g0.get(), g1.get());
    ensure_equals(d, 10);
}

template<>
template<>
void object::test<2>
()
{
    using geos::geom::Coordinate;

    std::string wkt0("POLYGON ((200 180, 60 140, 60 260, 200 180))");
    std::string wkt1("POINT (140 280)");
    double dist = 57.05597791103589;
    Coordinate p1(111.6923076923077, 230.46153846153845);
    Coordinate p2(140, 280);

    checkDistanceNearestPoints(wkt0, wkt1, dist, p1, p2);
}

template<>
template<>
void object::test<3>
()
{
    using geos::geom::Coordinate;

    std::string wkt0("POLYGON ((200 180, 60 140, 60 260, 200 180))");
    std::string wkt1("MULTIPOINT ((140 280), (140 320))");
    double dist = 57.05597791103589;
    Coordinate p1(111.6923076923077, 230.46153846153845);
    Coordinate p2(140, 280);

    checkDistanceNearestPoints(wkt0, wkt1, dist, p1, p2);
}

template<>
template<>
void object::test<4>
()
{
    using geos::geom::Coordinate;

    std::string wkt0("LINESTRING (100 100, 200 100, 200 200, 100 200, 100 100)");
    std::string wkt1("POINT (10 10)");
    double dist = 127.27922061357856;
    Coordinate p1(100, 100);
    Coordinate p2(10, 10);

    checkDistanceNearestPoints(wkt0, wkt1, dist, p1, p2);
}

template<>
template<>
void object::test<5>
()
{
    using geos::geom::Coordinate;

    std::string wkt0("POLYGON ((76 185, 125 283, 331 276, 324 122, 177 70, 184 155, 69 123, 76 185), (267 237, 148 248, 135 185, 223 189, 251 151, 286 183, 267 237))");
    std::string wkt1("LINESTRING (153 204, 185 224, 209 207, 238 222, 254 186)");
    double dist = 13.788860460124573;
    Coordinate p1(139.4956500724988, 206.78661188980183);
    Coordinate p2(153, 204);

    checkDistanceNearestPoints(wkt0, wkt1, dist, p1, p2);
}


//
template<>
template<>
void object::test<6>
()
{
    using geos::operation::distance::IndexedFacetDistance;

    std::string wkt0("POLYGON((100 200, 200 200, 200 100, 100 100, 100 200))");
    std::string wkt1("POINT(150 150)");
    GeomPtr g0(_wktreader.read(wkt0));
    GeomPtr g1(_wktreader.read(wkt1));
    double d = IndexedFacetDistance::distance(g0.get(), g1.get());
    ensure_equals(d, 50);
}


template<>
template<>
void object::test<7>
()
{
    using geos::operation::distance::IndexedFacetDistance;

    std::string wkt0("POLYGON((100 200, 200 200, 200 100, 100 100, 100 200))");
    std::string wkt1("POINT(150 150)");
    GeomPtr g0(_wktreader.read(wkt0));
    GeomPtr g1(_wktreader.read(wkt1));
    double d = IndexedFacetDistance::distance(g0.get(), g1.get());
    ensure_equals(d, 50);
}

template<>
template<>
void object::test<8>
()
{
    using geos::operation::distance::IndexedFacetDistance;
    std::string wkt0("POLYGON((100 200, 200 200, 200 100, 100 100, 100 200))");
    std::string wkt1("POINT(150 150)");
    GeomPtr g0(_wktreader.read(wkt0));
    GeomPtr g1(_wktreader.read(wkt1));
    IndexedFacetDistance ifd(g0.get());
    double expected_distance = 50;

    double actual_distance = ifd.distance(g1.get());
    ensure_equals(actual_distance, expected_distance);

    ensure(ifd.isWithinDistance(g1.get(), expected_distance));
    ensure(!ifd.isWithinDistance(g1.get(), expected_distance - 1e-6));
}

// Invalid polygon collapsed to a line
template<>
template<>
void object::test<9>
()
{
    using geos::operation::distance::IndexedFacetDistance;
    std::string wkt0("POLYGON((100 100, 200 200, 100 100, 100 100))");
    std::string wkt1("POINT(150 150)");
    GeomPtr g0(_wktreader.read(wkt0));
    GeomPtr g1(_wktreader.read(wkt1));
    IndexedFacetDistance ifd(g0.get());
    double d = ifd.distance(g1.get());
    ensure_equals("incorrect distance", d, 0.0, 0.001);

    auto nearestPts = ifd.nearestPoints(g1.get());
    ensure_equals("nearest points x", (*nearestPts)[0].x, (*nearestPts)[1].x, 0.00001);
    ensure_equals("nearest points y", (*nearestPts)[0].y, (*nearestPts)[1].y, 0.00001);
}

// Invalid polygon collapsed to a line
template<>
template<>
void object::test<10>
()
{
    std::size_t npoints = 1000; // vertices in sinstar test shape
    int ncells = 100; // number of columns/rows in test grid square

    double radius = 100.0;
    double amplitude = 0.3; // how far the sin deviates from perfect circle (0.0)
    double width = radius * (1+amplitude); // total radius of shape
    double cellsize = 2.0*width/ncells; // how big a cell is

    // Build a sine star of the requested size and prepare
    // the IndexedFacetDistance for it
    using geos::operation::distance::IndexedFacetDistance;
    std::unique_ptr<geos::geom::LineString> ls = makeSinCircle(npoints, radius, amplitude);
    IndexedFacetDistance ifd(ls.get());
    // std::string wkt = _wktwriter.write(ls.get());
    // std::cout << wkt << std::endl;

    // Build out the set of test points ahead of time so that
    // point creation overhead isn't included in the test timings
    std::vector<std::unique_ptr<geos::geom::Point>> pts;
    for (double x = -width; x < width; x += cellsize) {
        for (double y = -width; y < width; y += cellsize) {
            geos::geom::Coordinate c(x, y);
            pts.emplace(pts.end(), _factory->createPoint(c));
        }
    }

    geos::util::Profiler prof;

    enum modes {
        TestIndexedFacetDistance = 0,
        TestGeometryDistance = 1,
        TestBoth = 2
    };

    std::vector<std::string> profiles = {"TestIndexedFacetDistance",
                                         "TestGeometryDistance",
                                         "TestBoth"};

    bool perfTest = false;
    std::vector<size_t> m = {TestBoth};
    if (perfTest) {
        m.push_back(TestIndexedFacetDistance);
        m.push_back(TestGeometryDistance);
    }

    for (auto it = m.begin() ; it != m.end(); ++it)
    {
        prof.start(profiles[*it]);
        for (std::size_t j = 0; j < pts.size(); j++) {
            double dist_ifd = 0.0, dist_geom = 0.0;
            if (*it == TestIndexedFacetDistance || *it == TestBoth)
                dist_ifd = ifd.distance(pts[j].get());

            if (*it == TestGeometryDistance || *it == TestBoth)
                dist_geom = ls->distance(pts[j].get());

            if (*it == TestBoth)
                ensure_equals("distance", dist_ifd, dist_geom, 0.00001);
        }

        prof.stop(profiles[*it]);
    }
    if (perfTest) {
        std::cout << "npoints=" << npoints << " ncells=" << ncells << std::endl;
        std::cout << prof << std::endl;
    }
}

// EMPTY polygon
template<>
template<>
void object::test<11>
()
{
    using geos::operation::distance::IndexedFacetDistance;
    using geos::util::GEOSException;

    std::string wkt0("POLYGON EMPTY");
    std::string wkt1("POINT(150 150)");
    GeomPtr g0(_wktreader.read(wkt0));
    GeomPtr g1(_wktreader.read(wkt1));
    IndexedFacetDistance ifd0(g0.get());
    IndexedFacetDistance ifd1(g1.get());

    try {
        ifd0.distance(g1.get());
        fail("IndexedFacedDistance::distance did not throw on empty input");
    }
    catch (const GEOSException&) { }

    try {
        ifd0.nearestPoints(g1.get());
        fail("IndexedFacedDistance::nearestPoints did not throw on empty input");
    }
    catch (const GEOSException&) { }

    try {
        ifd1.distance(g0.get());
        fail("IndexedFacedDistance::distance did not throw on empty input");
    }
    catch (const GEOSException&) { }

    try {
        ifd1.nearestPoints(g0.get());
        fail("IndexedFacedDistance::nearestPoints did not throw on empty input");
    }
    catch (const GEOSException&) { }
}

// Test with Inf coords
template<>
template<>
void object::test<12>()
{
    auto g1 = _wktreader.read("POINT (0 0)");
    auto g2 = _wktreader.read("LINESTRING (3 Inf, 5 Inf)");

    IndexedFacetDistance ifd1(g1.get());

    auto pts = ifd1.nearestPoints(g2.get());
    ensure_equals(pts->size(), 2u);

    auto ls = _factory->createLineString(std::move(pts));
    ls->normalize();

    const auto& normPts = *ls->getCoordinatesRO();

    ensure_equals(normPts.getX(0), 0);
    ensure_equals(normPts.getY(0), 0);
    ensure_equals(normPts.getY(1), geos::DoubleInfinity);
}


// Indexed multiline with one element within line envelope.
template<>
template<>
void object::test<13>()
{
    checkWithinDistance(
        "MULTILINESTRING ((1 6, 1 1), (11 14, 11 11))",
        "LINESTRING (10 10, 10 20, 30 20)",
        2,
        true
    );
}

// Indexed multiline with one element within multiline envelope and close to inner line.
template<>
template<>
void object::test<14>()
{
    checkWithinDistance(
        "MULTILINESTRING ((1 6, 1 1), (16 16, 19 13))",
        "MULTILINESTRING ((10 10, 10 20, 30 20), (20 13, 20 16))",
        2,
        true
    );
}

// Indexed multiline with one element within line envelope.
template<>
template<>
void object::test<15>
()
{
    checkDistanceNearestPoints(
        "MULTILINESTRING ((1 6, 1 1), (11 15, 13 17))", 
        "LINESTRING (10 10, 10 20, 30 20)", 
        1, Coordinate{11, 15}, Coordinate{10, 15}
    );
}

// Indexed multiline with one element within line envelope.
template<>
template<>
void object::test<16>
()
{
    checkDistanceNearestPoints(
        "MULTILINESTRING ((6 6, 7 7), (100 100, 101 101))", 
        "LINESTRING (0 10, 10 0)", 
        1.41421, Coordinate{6, 6}, Coordinate{5, 5}
    );
}

// TODO: finish the tests by adding:
// 	LINESTRING - *all*
// 	MULTILINESTRING - *all*
// 	POLYGON - *all*
// 	MULTIPOLYGON - *all*
// 	COLLECTION - *all*

} // namespace tut

