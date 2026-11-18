// FixedPointNds.hpp - FixedPoint's arithmetic on the Nintendo DS: FlexPoint16's
// divide and square root on the DS's hardware divider and square root unit,
// and the DS's maths library for sin, pow and the rest. FixedPoint.hpp
// includes it in a DS build (__NDS__) instead of its portable versions.
//
// The DS's ARM9 has no divide instruction and no floating-point hardware,
// so a 64 / 32-bit division in software takes a few hundred cycles. The
// divider and square root unit do one in about 20 to 35.
//
// They're shared registers: a calculation is written to them and its result
// read back once they're no longer busy. Another thread starting one
// partway through would change the result. So these run with interrupts
// locked (calico switches threads on interrupts), for the few dozen cycles a
// calculation takes. libnds's own maths (divf32, sqrtf32, and the GL helpers
// that use them, such as gluLookAt) doesn't lock: code on another thread
// that calls it while FixedPoint arithmetic may run should lock interrupts
// around it too (Cyclone's DS app does around gluLookAt), or a calculation
// of ours could land partway through one of its.

#pragma once

#include <IntegerDivide.hpp>
#include <calico/arm/common.h>
#include <cmath>
#include <cstdint>
#include <nds/arm9/math.h>

namespace Flex::Math::detail {

// a x 2^shift / b, truncating toward zero: on the divider (IntegerDivideNds.hpp)
inline std::int32_t divideShifted(std::int32_t a, int shift, std::int32_t b) {
  return static_cast<std::int32_t>(divide64(static_cast<std::int64_t>(a) << shift, b).quotient);
}
// The square root of a 64-bit value, rounded down
inline std::int32_t squareRoot32(std::uint64_t value) {
  HardwareMathsLock lock;
  REG_SQRTCNT = SQRT_64;
  while (REG_SQRTCNT & SQRT_BUSY) {
  }
  REG_SQRT_PARAM = static_cast<std::int64_t>(value);
  while (REG_SQRTCNT & SQRT_BUSY) {
  }
  return static_cast<std::int32_t>(REG_SQRT_RESULT);
}

// sin, cos, exp, pow and atan2: the DS's own (newlib's), in software double
// precision. (CORE-MATH's, which the other builds use, needs __int128.)
namespace Transcendental {
inline double sin(double x) { return std::sin(x); }
inline double cos(double x) { return std::cos(x); }
inline double exp(double x) { return std::exp(x); }
inline double pow(double x, double y) { return std::pow(x, y); }
inline double atan2(double y, double x) { return std::atan2(y, x); }
} // namespace Transcendental

} // namespace Flex::Math::detail
