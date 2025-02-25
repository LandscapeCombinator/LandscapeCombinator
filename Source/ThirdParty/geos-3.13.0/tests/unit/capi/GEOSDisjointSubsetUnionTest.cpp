#include <tut/tut.hpp>
// geos
#include <geos_c.h>

#include "capi_test_utils.h"

namespace tut {
//
// Test Group
//

// Common data used in test cases.
struct test_capidisjointsubsetunion_data : public capitest::utility {};


typedef test_group<test_capidisjointsubsetunion_data> group;
typedef group::object object;

group test_capidisjointsubsetunion_group("capi::GEOSDisjointSubsetUnion");

//
// Test Cases
//


template<>
template<>
void object::test<1>()
{
    input_ = fromWKT("POLYGON EMPTY");
    GEOSSetSRID(input_, 1234);

    result_ = GEOSDisjointSubsetUnion(input_);

    ensure(GEOSisEmpty(result_));
    ensure_equals(GEOSGetSRID(input_), GEOSGetSRID(result_));
}

template<>
template<>
void object::test<2>()
{
    input_ = fromWKT("MULTIPOLYGON (((0 0, 1 0, 1 1, 0 1, 0 0)), ((1 0, 2 0, 2 1, 1 1, 1 0)), ((3 3, 4 3, 4 4, 3 3)))");
    expected_ = fromWKT("MULTIPOLYGON (((0 0, 1 0, 2 0, 2 1, 1 1, 0 1, 0 0)), ((3 3, 4 3, 4 4, 3 3)))");
    result_ = GEOSDisjointSubsetUnion(input_);
    ensure_geometry_equals(result_, expected_);
}

template <>
template <>
void object::test<3>() {
    input_ = fromWKT("MULTICURVE ((0 0, 1 1), CIRCULARSTRING (1 1, 2 0, 3 1), (5 5, 8 8))");
    ensure(input_);

    result_ = GEOSDisjointSubsetUnion(input_);
    ensure("curved geometry not supported", result_ == nullptr);
}

} // namespace tut

