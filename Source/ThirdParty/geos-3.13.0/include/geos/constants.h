/**********************************************************************
 *
 * constants.h
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2018 Vicky Vergara
 * Copyright (C) 2009 Mateusz Loskot
 * Copyright (C) 2005-2009 Refractions Research Inc.
 * Copyright (C) 2001-2009 Vivid Solutions Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Public Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 *********************************************************************/

#pragma once

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX 1
typedef __int64 int64;
#endif
#endif

#include <cmath>
#include <limits>
#include <cinttypes>
#include <cstddef> // for std::size_t

namespace geos {

constexpr double MATH_PI = 3.14159265358979323846;

// Some handy constants
constexpr double DoubleNotANumber = std::numeric_limits<double>::quiet_NaN();
constexpr double DoubleMax = (std::numeric_limits<double>::max)();
constexpr double DoubleInfinity = (std::numeric_limits<double>::infinity)();
constexpr double DoubleNegInfinity = (-(std::numeric_limits<double>::infinity)());
constexpr double DoubleEpsilon = std::numeric_limits<double>::epsilon();

constexpr std::size_t NO_COORD_INDEX = std::numeric_limits<std::size_t>::max();
constexpr std::size_t INDEX_UNKNOWN = std::numeric_limits<std::size_t>::max();

} // namespace geos

