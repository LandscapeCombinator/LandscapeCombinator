//
// Test Suite for C-API GEOSintersection

#include "capi_test_utils.h"


namespace tut {
//
// Test Group
//

// Common data used in test cases.
struct test_capigeosintersection_data : public capitest::utility
{
    test_capigeosintersection_data() {
    }
};

typedef test_group<test_capigeosintersection_data> group;
typedef group::object object;

group test_capigeosintersection_group("capi::GEOSIntersection");

//
// Test Cases
//

template<>
template<>
void object::test<1>
()
{
    geom1_ = GEOSGeomFromWKT("POLYGON EMPTY");
    geom2_ = GEOSGeomFromWKT("POLYGON EMPTY");

    ensure(nullptr != geom1_);
    ensure(nullptr != geom2_);

    geom3_ = GEOSIntersection(geom1_, geom2_);
    ensure(nullptr != geom3_);
    ensure_geometry_equals(geom3_, "POLYGON EMPTY");
}

template<>
template<>
void object::test<2>
()
{
    geom1_ = GEOSGeomFromWKT("POLYGON((1 1,1 5,5 5,5 1,1 1))");
    geom2_ = GEOSGeomFromWKT("POINT(2 2)");

    ensure(nullptr != geom1_);
    ensure(nullptr != geom2_);

    geom3_ = GEOSIntersection(geom1_, geom2_);
    ensure(nullptr != geom3_);
    ensure_geometry_equals(geom3_, "POINT (2 2)");
}

template<>
template<>
void object::test<3>
()
{
    geom1_ = GEOSGeomFromWKT("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))");
    geom2_ = GEOSGeomFromWKT("POLYGON((-1 1,-1 2,2 2,2 1,-1 1))");
    expected_ = GEOSGeomFromWKT("POLYGON ((0 1, 0 2, 2 2, 2 1, 0 1))");

    ensure(nullptr != geom1_);
    ensure(nullptr != geom2_);

    geom3_ = GEOSIntersection(geom1_, geom2_);

    ensure(nullptr != geom3_);
    ensure_geometry_equals(geom3_, expected_, 0.1);
}

/* See http://trac.osgeo.org/geos/ticket/719 */
template<>
template<>
void object::test<4>
()
{
    geom1_ = GEOSGeomFromWKT("MULTIPOLYGON(((0 0,5 10,10 0,0 0),(1 1,1 2,2 2,2 1,1 1),(100 100,100 102,102 102,102 100,100 100)))");
    geom2_ = GEOSGeomFromWKT("POLYGON((0 1,0 2,10 2,10 1,0 1))");

    ensure(nullptr != geom1_);
    ensure(nullptr != geom2_);

    geom3_ = GEOSIntersection(geom1_, geom2_);
    GEOSNormalize(geom3_);

    ensure(nullptr != geom3_);

    expected_ = GEOSGeomFromWKT("GEOMETRYCOLLECTION (LINESTRING (1 2, 2 2), LINESTRING (2 1, 1 1), POLYGON ((0.5 1, 1 2, 1 1, 0.5 1)), POLYGON ((9 2, 9.5 1, 2 1, 2 2, 9 2)))");
    GEOSNormalize(expected_);
    ensure(GEOSEqualsExact(expected_, geom3_, 0.00001));
}

// https://trac.osgeo.org/geos/ticket/1134
template<>
template<>
void object::test<5>
()
{
    geom1_ = GEOSGeomFromWKT("LINESTRING (nan 0.0000000000000000, 0.0000000000000000 1.0000000000000000, 1.0000000000000000 1.0000000000000000, 1.0000000000000000 0.0000000000000000, nan 0.0000000000000000)");
    geom2_ = GEOSGeomFromWKT("POLYGON ((-86060443046427184.0000000000000000 20820223.5124294497072697, -86060443046427184.0000000000000000 -20820223.5124294497072697, -86060443004786720.0000000000000000 -20820223.5124294497072697, -86060443004786720.0000000000000000 20820223.5124294497072697, -86060443046427184.0000000000000000 20820223.5124294497072697))");

    ensure(geom1_);
    ensure(geom2_);

    geom3_ = GEOSIntersection(geom1_, geom2_);
    // No memory leaked
}

// https://trac.osgeo.org/geos/ticket/1134
template<>
template<>
void object::test<6>
()
{
    geom1_ = GEOSGeomFromWKT("LINESTRING (nan 0.0000000000000000, 0.0000000000000000 nan)");
    geom2_ = GEOSGeomFromWKT("POLYGON ((-86060443046427184.0000000000000000 20820223.5124294497072697, -86060443046427184.0000000000000000 -20820223.5124294497072697, -86060443004786720.0000000000000000 -20820223.5124294497072697, -86060443004786720.0000000000000000 20820223.5124294497072697, -86060443046427184.0000000000000000 20820223.5124294497072697))");

    ensure(geom1_);
    ensure(geom2_);

    geom3_ = GEOSIntersection(geom1_, geom2_);
    // No memory leaked
}

// https://github.com/libgeos/geos/pull/790
template<>
template<>
void object::test<7>
()
{
    geom1_ = GEOSGeomFromWKT("POLYGON ((1 0, 1 1, 0 1, 0 0, 1 0))");
    geom2_ = GEOSGeomFromWKT("POLYGON ((1 2, 1 3, 0 3, 0 2, 1 2))");

    result_ = GEOSIntersection(geom1_, geom2_);

    ensure(!std::fetestexcept(FE_INVALID));
}

template<>
template<>
void object::test<8>()
{
    geom1_ = fromWKT("CIRCULARSTRING (0 0, 1 1, 2 0)");
    geom2_ = fromWKT("LINESTRING (1 0, 2 1)");

    ensure(geom1_);
    ensure(geom2_);

    result_ = GEOSIntersection(geom1_, geom2_);
    ensure("curved geometry not supported", result_ == nullptr);
}

// https://github.com/libgeos/geos/issues/1074
// Make sure no crash occurs when LineIntersector has mixed-dimensionality
// inputs and substitutes a segment endpoint for intersection point
template<>
template<>
void object::test<9>()
{
    geom1_ = fromWKT("GEOMETRYCOLLECTION(POINT(0 0 59.083333333333336), POINT(1 1 59.083333333333336), LINESTRING(2 4 111 1, 2 223 22 2), POLYGON((42 -2.222222223222222e+155 111 1, 2 3 1 1, 2 4 111 NaN, 42 -2.222222223222222e+155 111 NaN)), POLYGON((2.2222222212222224e+149 4 111 1, 42 -2.222222222222222e+149 22 2, 2 4 111 1, 22 2222222222222223 22 NaN, 2.2222222212222224e+149 4 111 NaN)))");
    geom2_ = fromWKT("LINESTRING (1.6409301755752343e+149 -5.298190649085575e+148, 1.6666666660752404e+149 -5.55555555396982e+148)");

    result_ = GEOSIntersection(geom1_, geom2_);
    ensure(result_ != nullptr);
    ensure("HasZ", GEOSHasZ(result_));
    ensure("HasM", GEOSHasM(result_));
}

} // namespace tut

