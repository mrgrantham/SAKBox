// CorrectlyRounded.hpp - maths functions that give the same result on every
// platform.
//
// The system's sin, cos, exp, pow and the like (Apple's libm, glibc,
// Microsoft's) aren't required to be correctly rounded, and each gets the
// last bit wrong on different inputs, so the same program gives slightly
// different results on different platforms. These are CORE-MATH's
// (https://core-math.gitlabpages.inria.fr/), which are correctly rounded:
// each returns the double nearest the exact result, the one right answer, so
// every platform and compiler agrees. sqrt, fabs and fmod need nothing here:
// IEEE 754 already makes them exact everywhere.
//
// Use them where results must match across machines (Cyclone's real_sin and
// friends, FlexReal's flex_sin and friends), and build with fused
// multiply-adds off (.bazelrc's -ffp-contract=off), which change results
// too. See the top-level README's "Floating-point results across platforms".

#pragma once

extern "C" {
double cr_sin(double);
double cr_cos(double);
double cr_exp(double);
double cr_log(double);
double cr_pow(double, double);
double cr_atan2(double, double);
double cr_asin(double);
double cr_acos(double);
}

namespace Flex::Math::CorrectlyRounded {

inline double sin(double x) { return cr_sin(x); }
inline double cos(double x) { return cr_cos(x); }
inline double exp(double x) { return cr_exp(x); }
inline double log(double x) { return cr_log(x); }
inline double pow(double x, double y) { return cr_pow(x, y); }
inline double atan2(double y, double x) { return cr_atan2(y, x); }
inline double asin(double x) { return cr_asin(x); }
inline double acos(double x) { return cr_acos(x); }

} // namespace Flex::Math::CorrectlyRounded
