// FlexPrecision.hpp - the core of flexmath.
//
// A Precision describes how finely (fractional bits) and how far (integer
// bits) a value is allowed to resolve. quantize() applies one to a value.
//
// flexmath never stores values at reduced precision by default. Values are
// kept at full precision and a Precision is applied when a value is read or
// committed (see FlexGroup). That is what makes precision cheap to change at
// runtime: nothing has to visit or update the stored values.

#pragma once

#include <cmath>

namespace Flex::Math {

// How a value between two representable values is snapped.
enum class Rounding {
  TowardZero, // drop the excess: 1.75 -> 1.5 and -1.75 -> -1.5 at 1 bit
  Nearest,    // closest value; exact halves round away from zero (-1.5 -> -2 at 0 bits)
};

// What happens to values outside the integer range set by Precision::intBits.
enum class Overflow {
  None,     // unlimited range; intBits is ignored
  Saturate, // clamp to the most negative or most positive representable value
  Wrap,     // wrap around like a fixed-width two's complement register
};

// How finely and how far a value can resolve.
//
//   fracBits  Values snap to multiples of step() = 2^-fracBits.
//             4 -> steps of 1/16, 0 -> whole numbers, -1 -> steps of 2.
//             52 or more leaves a double untouched (see kFullPrecision).
//   intBits   With overflow set, values are limited to
//             [-2^intBits, 2^intBits): intBits = 4 behaves like a 5-bit
//             signed register (-16 .. 15.x). Ignored when overflow is None.
//
// Examples:
//   Precision{.fracBits = 2}                                  // quarters
//   Precision{.fracBits = 4, .rounding = Rounding::Nearest}   // nearest 1/16
//   Precision{.intBits = 4, .overflow = Overflow::Wrap}       // 16 wraps to -16
struct Precision {
  int fracBits = 52;
  int intBits = 0;
  Rounding rounding = Rounding::TowardZero;
  Overflow overflow = Overflow::None;

  // Distance between neighbouring representable values: 2^-fracBits.
  double step() const { return std::ldexp(1.0, -fracBits); }

  bool operator==(const Precision &) const = default;
};

// Full double precision with unlimited range: quantize() leaves values
// unchanged. The default for groups and for FlexReal's active precision.
inline constexpr Precision kFullPrecision{};

// Returns `value` as seen at `precision`: snapped to a multiple of
// 2^-fracBits using the precision's rounding, then limited to the integer
// range according to its overflow mode.
//
// - Snapping twice gives the same result as snapping once, and snapping never
//   reorders values (a <= b implies quantize(a) <= quantize(b)).
// - Infinities and NaN are returned unchanged.
// - Magnitudes of 2^(52 - fracBits) or more are already exact multiples of
//   the step and are not snapped.
//
// FlexPoint::quantized() implements the same rules with integer arithmetic,
// and the two agree whenever fracBits <= 32 (a FlexPoint cannot represent
// steps finer than 2^-32).
inline double quantize(double value, const Precision &precision) {
  if (!std::isfinite(value)) {
    return value;
  }
  // Values this large are already exact multiples of the step
  if (precision.fracBits < 52 && std::fabs(value) < std::ldexp(1.0, 52 - precision.fracBits)) {
    const double steps = std::ldexp(value, precision.fracBits);
    value = std::ldexp(precision.rounding == Rounding::Nearest ? std::round(steps) : std::trunc(steps),
                       -precision.fracBits);
  }
  if (precision.overflow == Overflow::None) {
    return value;
  }
  const double limit = std::ldexp(1.0, precision.intBits);
  if (precision.overflow == Overflow::Saturate) {
    const double highest = precision.fracBits < 52 ? limit - precision.step() : std::nextafter(limit, 0.0);
    return value < -limit ? -limit : (value > highest ? highest : value);
  }
  const double span = 2.0 * limit;
  return value - span * std::floor((value + limit) / span);
}

} // namespace Flex::Math
