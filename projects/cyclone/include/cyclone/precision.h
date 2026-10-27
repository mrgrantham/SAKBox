/*
 * Interface file for code that changes when the core's precision is
 * altered.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

/**
 * @file
 *
 * Because Cyclone is designed to work at either single or double
 * precision, mathematical functions such as sqrt cannot be used
 * in the source code or headers. This file provides defines for
 * the real number type and mathematical formulae that work on it.
 *
 * @note All the contents of this file need to be changed to compile
 * Cyclone at a different precision.
 */
#ifndef CYCLONE_PRECISION_H
#define CYCLONE_PRECISION_H

#include <float.h>

#if defined(CYCLONE_FLEX_REAL)
#include <FlexReal.hpp>
#elif !defined(CYCLONE_SINGLE_PRECISION)
#include <CorrectlyRounded.hpp>
#endif

namespace cyclone {

#if defined(CYCLONE_FLEX_REAL)
/**
 * SAKBox addition: real snaps to a precision that can change at
 * runtime (flexmath's FlexReal, see projects/flexmath).
 */
#define FLEX_PRECISION
typedef Flex::Math::FlexReal real;
#define REAL_MAX DBL_MAX
#define real_sqrt Flex::Math::flex_sqrt
#define real_abs Flex::Math::flex_abs
#define real_sin Flex::Math::flex_sin
#define real_cos Flex::Math::flex_cos
#define real_exp Flex::Math::flex_exp
#define real_pow Flex::Math::flex_pow
#define real_fmod Flex::Math::flex_fmod
#define real_atan2 Flex::Math::flex_atan2
#define real_epsilon DBL_EPSILON
#define R_PI 3.14159265358979
#elif defined(CYCLONE_SINGLE_PRECISION)
/**
 * SAKBox: CYCLONE_SINGLE_PRECISION picks single precision (upstream had
 * `#elif 0`, so only by editing this file). The DS build uses it: the DS
 * has no floating-point hardware, and float is the cheaper of the two to
 * do in software.
 */
/**
 * Defines we're in single precision mode, for any code
 * that needs to be conditionally compiled.
 */
#define SINGLE_PRECISION

/**
 * Defines a real number precision. Cyclone can be compiled in
 * single or double precision versions. By default single precision is
 * provided.
 */
typedef float real;

/** Defines the highest value for the real number. */
#define REAL_MAX FLT_MAX

/** Defines the precision of the square root operator. */
#define real_sqrt sqrtf
/** Defines the precision of the absolute magnitude operator. */
#define real_abs fabsf
/** Defines the precision of the sine operator. */
#define real_sin sinf

/** Defines the precision of the cosine operator. */
#define real_cos cosf

/** Defines the precision of the exponent operator. */
#define real_exp expf
/** Defines the precision of the power operator. */
#define real_pow powf

/** Defines the precision of the floating point modulo operator. */
#define real_fmod fmodf

#define real_atan2 atan2f

/** Defines the number e on which 1+e == 1 **/
#define real_epsilon FLT_EPSILON

#define R_PI 3.14159f
#else
#define DOUBLE_PRECISION
typedef double real;
#define REAL_MAX DBL_MAX
// SAKBox: sin, cos, exp, pow and atan2 are correctly rounded, so the engine
// gives the same results on every platform (flexmath's CorrectlyRounded.hpp);
// sqrt, fabs and fmod are exact anyway
#define real_sqrt sqrt
#define real_abs fabs
#define real_sin Flex::Math::CorrectlyRounded::sin
#define real_cos Flex::Math::CorrectlyRounded::cos
#define real_exp Flex::Math::CorrectlyRounded::exp
#define real_pow Flex::Math::CorrectlyRounded::pow
#define real_fmod fmod
#define real_atan2 Flex::Math::CorrectlyRounded::atan2
#define real_epsilon DBL_EPSILON
#define R_PI 3.14159265358979
#endif

/**
 * SAKBox addition: the type of a time step. In the flex build it is a
 * double, so the time step keeps full precision instead of snapping
 * like real; integrate() derives per-frame factors from it inside
 * CYCLONE_FULL_PRECISION_SCOPE, and only the body's state snaps.
 * Snapping the time step or the drag factor would bias every frame the
 * same way (see projects/flexmath/README.md). Otherwise it is just real.
 */
#if defined(CYCLONE_FLEX_REAL)
typedef double real_duration;
#define CYCLONE_FULL_PRECISION_SCOPE                                                               \
  Flex::Math::ScopedActivePrecision cyclone_full_precision_scope(Flex::Math::kFullPrecision)
#else
typedef real real_duration;
#define CYCLONE_FULL_PRECISION_SCOPE                                                               \
  do {                                                                                             \
  } while (0)
#endif
} // namespace cyclone

#endif // CYCLONE_PRECISION_H
