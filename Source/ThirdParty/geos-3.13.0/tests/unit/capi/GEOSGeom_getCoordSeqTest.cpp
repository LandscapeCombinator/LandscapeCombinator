#include <tut/tut.hpp>
// geos
#include <geos_c.h>

#include "capi_test_utils.h"

namespace tut {
//
// Test Group
//

struct test_geosgeom_getcoordseq_data : public capitest::utility {};

typedef test_group<test_geosgeom_getcoordseq_data> group;
typedef group::object object;

group test_geosgeom_getcoordseq("capi::GEOSGeom_getCoordSeq");

template<>
template<>
void object::test<1>
()
{
    input_ = fromWKT("LINESTRING (1 2, 4 5, 9 -2)");
    const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq(input_);

    double x = -1;
    double y = -1;
    GEOSCoordSeq_getXY(seq,  2, &x, &y);

    ensure_equals(x, 9);
    ensure_equals(y, -2);
}

template<>
template<>
void object::test<2>()
{
    input_ = fromWKT("POLYGON ((1 1, 2 1, 2 2, 1 1))");
    const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq(input_);

    ensure(seq == nullptr); // can't get seq from Polygon
}

template<>
template<>
void object::test<3>()
{
    input_ = fromWKT("POINT (3 8)");
    const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq(input_);

    double x = -1;
    double y = -1;
    GEOSCoordSeq_getXY(seq,  0, &x, &y);

    ensure_equals(x, 3);
    ensure_equals(y, 8);
}

template<>
template<>
void object::test<4>()
{
    input_ = fromWKT("CIRCULARSTRING (0 0, 1 1, 2 0)");
    const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq(input_);

    ensure(seq);

    unsigned int size;
    ensure(GEOSCoordSeq_getSize(seq, &size));
    ensure_equals(size, 3u);
}

} // namespace tut

