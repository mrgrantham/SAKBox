// FixedPoint.hpp - flexmath's fixed-point value types: FlexPoint (Q32.32)
// and FlexPoint16 (Q16.16).
//
// FixedPoint<IntBits, FracBits> is a signed number held as an integer
// counting units of 2^-FracBits: 32 bits in all (FlexPoint16) or 64
// (FlexPoint). All arithmetic is integer arithmetic, so results are
// bit-identical on every machine and compiler.
//
//   FlexPoint (Q32.32)   range about +-2^31, step 2^-32 (2.3e-10); 64-bit
//   FlexPoint16 (Q16.16) range about +-32768, step 2^-16 (1.5e-5); 32-bit,
//                        fast on 32-bit processors (a multiply is one 32 x 32
//                        -> 64-bit multiply and a shift), such as the
//                        Nintendo DS's, which has no floating-point hardware
//
// Arithmetic rounding:
// - + and - are exact.
// - * keeps the exact product's high bits, rounding toward negative infinity
//   in the last place (2^-FracBits).
// - / truncates toward zero in the last place. Dividing by zero is undefined
//   behaviour (an integer division by zero), so check divisors first.
// - Exceeding the range, in construction or arithmetic, overflows the
//   underlying integer.
//
// A product or quotient needs an integer twice as wide as the value for a
// moment: 64 bits for FlexPoint16, and 128 for FlexPoint, which uses GCC's
// and Clang's __int128 where the compiler has it (64-bit targets) and
// portable code built from 64-bit arithmetic where it doesn't (32-bit
// targets, such as the DS and 3DS, and MSVC). On the DS, FlexPoint16's
// divide and square root use its hardware divider and square root unit
// (FixedPointNds.hpp).
//
// FlexPoint.hpp adds printing with fmt ("{}", "{:x}"); this header needs
// nothing but the standard library, so it builds anywhere.
//
// It converts implicitly to and from double (and mixes with other numbers in
// arithmetic and comparisons, as FlexReal does), so it can stand in for a
// `real` typedef: Cyclone builds on FlexPoint and FlexPoint16 this way (see
// the README's "Using flexmath with Cyclone").

#pragma once

#include <FlexPrecision.hpp>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <type_traits>

#if defined(__SIZEOF_INT128__)
#define FLEXMATH_HAS_INT128 1
#endif

namespace Flex::Math {

namespace detail {

// The wide arithmetic FixedPoint needs, for each size: a product shifted
// back down, a dividend shifted up and divided, and an integer square root.

// a x b / 2^shift, rounding toward negative infinity
inline std::int32_t multiplyShifted(std::int32_t a, std::int32_t b, int shift) {
  return static_cast<std::int32_t>((static_cast<std::int64_t>(a) * b) >> shift);
}

// The 64-bit versions need 128 bits for a moment. Without a 128-bit type,
// an unsigned 128-bit value is two 64-bit halves:
struct Unsigned128 {
  std::uint64_t high, low;
};

// The full product of two unsigned 64-bit values, from 32-bit pieces
inline Unsigned128 multiplyWide(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t aLow = a & 0xFFFFFFFFu, aHigh = a >> 32;
  const std::uint64_t bLow = b & 0xFFFFFFFFu, bHigh = b >> 32;
  const std::uint64_t lowLow = aLow * bLow, lowHigh = aLow * bHigh;
  const std::uint64_t highLow = aHigh * bLow, highHigh = aHigh * bHigh;
  const std::uint64_t middle = (lowLow >> 32) + (lowHigh & 0xFFFFFFFFu) + (highLow & 0xFFFFFFFFu);
  return {highHigh + (lowHigh >> 32) + (highLow >> 32) + (middle >> 32),
          (middle << 32) | (lowLow & 0xFFFFFFFFu)};
}

// Two's complement negation
inline Unsigned128 negate(Unsigned128 value) {
  const std::uint64_t low = ~value.low + 1;
  return {~value.high + (low == 0 ? 1 : 0), low};
}

inline std::uint64_t sizeOf(std::int64_t value) {
  return value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
}

// a x b / 2^shift (0 < shift < 64), rounding toward negative infinity,
// without a 128-bit type
inline std::int64_t multiplyShiftedPortable(std::int64_t a, std::int64_t b, int shift) {
  // The product of the sizes, given the sign, then shifted down as a signed
  // 128-bit value (an arithmetic shift, so it rounds toward negative
  // infinity)
  Unsigned128 product = multiplyWide(sizeOf(a), sizeOf(b));
  if ((a < 0) != (b < 0))
    product = negate(product);
  return static_cast<std::int64_t>((product.low >> shift) | (product.high << (64 - shift)));
}

// a x 2^shift / b (0 < shift < 64), truncating toward zero and keeping the
// low 64 bits of a quotient too big for them, without a 128-bit type
inline std::int64_t divideShiftedPortable(std::int64_t a, int shift, std::int64_t b) {
  // Long division of the sizes, a bit at a time, then the sign
  const std::uint64_t divisor = sizeOf(b);
  const Unsigned128 dividend = {sizeOf(a) >> (64 - shift), sizeOf(a) << shift};
  std::uint64_t quotient = 0, remainder = 0;
  for (int bit = 127; bit >= 0; bit--) {
    const bool carry = remainder >> 63;
    remainder =
        (remainder << 1) | ((bit >= 64 ? dividend.high >> (bit - 64) : dividend.low >> bit) & 1);
    quotient <<= 1;
    if (carry || remainder >= divisor) {
      remainder -= divisor;
      quotient |= 1;
    }
  }
  return static_cast<std::int64_t>((a < 0) != (b < 0) ? 0 - quotient : quotient);
}

#if defined(FLEXMATH_HAS_INT128)
// __extension__ marks the use as deliberate, so -pedantic-errors allows it
__extension__ typedef __int128 Int128;

inline std::int64_t multiplyShifted(std::int64_t a, std::int64_t b, int shift) {
  return static_cast<std::int64_t>((static_cast<Int128>(a) * b) >> shift);
}

inline std::int64_t divideShifted(std::int64_t a, int shift, std::int64_t b) {
  return static_cast<std::int64_t>((static_cast<Int128>(a) << shift) / b);
}
#else
inline std::int64_t multiplyShifted(std::int64_t a, std::int64_t b, int shift) {
  return multiplyShiftedPortable(a, b, shift);
}

inline std::int64_t divideShifted(std::int64_t a, int shift, std::int64_t b) {
  return divideShiftedPortable(a, shift, b);
}
#endif

// The integer square root of `value`, rounded down (Newton's method)
inline std::uint64_t squareRoot(std::uint64_t value) {
  if (value < 2)
    return value;
  std::uint64_t root = static_cast<std::uint64_t>(std::sqrt(static_cast<double>(value)));
  // The double's guess can be a little out either way; correct it exactly
  while (root > 0 && root > value / root)
    root--;
  while ((root + 1) <= value / (root + 1))
    root++;
  return root;
}

} // namespace detail
} // namespace Flex::Math

#if defined(__NDS__)
// The DS's divider and square root unit, for FlexPoint16 (divideShifted and
// squareRoot32 below), and its maths library (sin, pow, ...: CORE-MATH needs
// __int128, which the DS's 32-bit compiler doesn't have)
#include <FixedPointNds.hpp>
#else
#include <CorrectlyRounded.hpp>

namespace Flex::Math::detail {
// sin, cos, exp, pow and atan2 for the functions below: correctly rounded,
// so the same everywhere
namespace Transcendental = CorrectlyRounded;

inline std::int32_t divideShifted(std::int32_t a, int shift, std::int32_t b) {
  return static_cast<std::int32_t>((static_cast<std::int64_t>(a) << shift) / b);
}

// The square root of `value` (which holds 2 x FracBits fraction bits), as a
// 32-bit fixed-point value
inline std::int32_t squareRoot32(std::uint64_t value) {
  return static_cast<std::int32_t>(squareRoot(value));
}
} // namespace Flex::Math::detail
#endif

namespace Flex::Math {

template <int IntBits, int FracBits> class FixedPoint {
  static_assert(IntBits + FracBits == 32 || IntBits + FracBits == 64,
                "a FixedPoint is 32 or 64 bits in all");

public:
  // The underlying integer type: the value multiplied by 2^FracBits.
  using Fixed = std::conditional_t<IntBits + FracBits == 32, std::int32_t, std::int64_t>;
  static constexpr int kIntegerBits = IntBits;
  static constexpr int kFractionalBits = FracBits;

  // Zero.
  constexpr FixedPoint() {}

  // From an integer, exactly. `value` must be within the range.
  template <typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
  constexpr FixedPoint(Integer value) : valueFixed_(static_cast<Fixed>(value) * kOne) {}

  // From floating point, rounded to the nearest step (halves away from zero).
  // `value` must be finite and within the range. Plain arithmetic (scaling
  // by a power of two is exact), so a constant such as (real)0.5 becomes its
  // fixed-point value when compiling: without floating-point hardware, as on
  // the DS, converting at run time is slow.
  template <typename Floating,
            std::enable_if_t<std::is_floating_point<Floating>::value, bool> = true>
  constexpr FixedPoint(Floating value) : valueFixed_(nearestFixed(static_cast<double>(value))) {}

  // From the raw fixed-point representation (the value times 2^FracBits),
  // e.g. one previously read with valueFixed() or loaded from a save or
  // network.
  static constexpr FixedPoint fromFixed(Fixed valueFixed) {
    FixedPoint point;
    point.valueFixed_ = valueFixed;
    return point;
  }

  // The smallest step, and the largest and smallest values.
  static constexpr FixedPoint epsilon() { return fromFixed(1); }
  static constexpr FixedPoint max() { return fromFixed(std::numeric_limits<Fixed>::max()); }
  static constexpr FixedPoint lowest() { return fromFixed(std::numeric_limits<Fixed>::min()); }

  // The value as a double (exact for FlexPoint16, and for FlexPoint while
  // |value| < 2^21).
  double value() const { return std::ldexp(static_cast<double>(valueFixed_), -kFractionalBits); }
  operator double() const { return value(); }

  // The raw fixed-point representation: the value times 2^FracBits.
  constexpr Fixed valueFixed() const { return valueFixed_; }

  constexpr FixedPoint operator+(const FixedPoint &b) const {
    return fromFixed(valueFixed_ + b.valueFixed_);
  }
  constexpr FixedPoint operator-(const FixedPoint &b) const {
    return fromFixed(valueFixed_ - b.valueFixed_);
  }
  constexpr FixedPoint operator-() const { return fromFixed(-valueFixed_); }

  // Rounds toward negative infinity in the last place (see the top).
  FixedPoint operator*(const FixedPoint &b) const {
    return fromFixed(detail::multiplyShifted(valueFixed_, b.valueFixed_, kFractionalBits));
  }

  // Truncates toward zero in the last place. `b` must not be zero.
  FixedPoint operator/(const FixedPoint &b) const {
    return fromFixed(detail::divideShifted(valueFixed_, kFractionalBits, b.valueFixed_));
  }

  FixedPoint &operator+=(const FixedPoint &b) { return *this = *this + b; }
  FixedPoint &operator-=(const FixedPoint &b) { return *this = *this - b; }
  FixedPoint &operator*=(const FixedPoint &b) { return *this = *this * b; }
  FixedPoint &operator/=(const FixedPoint &b) { return *this = *this / b; }

  // Exact comparison of the underlying fixed-point values. To compare at a
  // reduced precision, compare quantized values:
  //   a.quantized(p) == b.quantized(p)
  constexpr auto operator<=>(const FixedPoint &b) const = default;
  constexpr bool operator==(const FixedPoint &b) const = default;

  // This value as seen at `precision`, following the same rules as
  // quantize(): snap to 2^-fracBits with the precision's rounding, then apply
  // the integer range. Computed with integer arithmetic, so it is exact and
  // deterministic. fracBits of FracBits or more leave the fraction
  // unchanged, and intBits of IntBits - 1 or more leave the range unchanged
  // (the type's own limit).
  FixedPoint quantized(const Precision &precision) const;

private:
  static constexpr Fixed kOne = Fixed{1} << kFractionalBits;
  static constexpr double kScale = static_cast<double>(kOne);

  // `value` x 2^FracBits, to the nearest integer (halves away from zero).
  // From 2^52 up, a double is a whole number already, and adding a half
  // could round it up.
  static constexpr Fixed nearestFixed(double value) {
    const double scaled = value * kScale;
    const double half = (scaled < 0 ? -scaled : scaled) < 0x1p52 ? 0.5 : 0.0;
    return static_cast<Fixed>(scaled < 0 ? scaled - half : scaled + half);
  }

  Fixed valueFixed_{0};
};

using FlexPoint = FixedPoint<32, 32>;
using FlexPoint16 = FixedPoint<16, 16>;

template <int IntBits, int FracBits>
FixedPoint<IntBits, FracBits>
FixedPoint<IntBits, FracBits>::quantized(const Precision &precision) const {
  constexpr int kBits = IntBits + FracBits;
  Fixed raw = valueFixed_;

  // Fractional precision: keep multiples of 2^-fracBits
  int drop = kFractionalBits - precision.fracBits;
  if (drop > kBits - 2) {
    drop = kBits - 2;
  }
  Fixed unit = 1;
  if (drop > 0) {
    unit = Fixed{1} << drop;
    if (precision.rounding == Rounding::Nearest) {
      // Halves round away from zero, matching std::round in quantize()
      raw = raw >= 0 ? ((raw + unit / 2) / unit) * unit : -(((-raw + unit / 2) / unit) * unit);
    } else if (precision.rounding == Rounding::Stochastic) {
      // Round down to the grid (masking floors in two's complement), then up
      // with probability remainder / unit, using `drop` random bits
      const Fixed below = raw & ~(unit - 1);
      const Fixed remainder = raw - below;
      raw = below;
      if (remainder != 0 &&
          static_cast<Fixed>(detail::nextStochasticRoundingBits() >> (64 - drop)) < remainder) {
        raw += unit;
      }
    } else {
      raw = (raw / unit) * unit; // integer division truncates toward zero
    }
  }

  // Integer range; the type itself only reaches +-2^(IntBits - 1)
  if (precision.overflow == Overflow::None || precision.intBits >= IntBits - 1) {
    return fromFixed(raw);
  }
  const Fixed limit = Fixed{1} << (precision.intBits + kFractionalBits);
  if (precision.overflow == Overflow::Saturate) {
    const Fixed highest = limit - unit;
    return fromFixed(raw < -limit ? -limit : (raw > highest ? highest : raw));
  }
  // Wrap within -limit to limit. The span, 2 x limit, is a power of two that
  // divides 2^64, so (raw + limit) mod span can be taken in unsigned 64-bit
  // arithmetic, which wraps the same way
  const std::uint64_t span = static_cast<std::uint64_t>(limit) * 2;
  const std::uint64_t wrapped = (static_cast<std::uint64_t>(static_cast<std::int64_t>(raw)) +
                                 static_cast<std::uint64_t>(limit)) &
                                (span - 1);
  return fromFixed(static_cast<Fixed>(static_cast<std::int64_t>(wrapped) - limit));
}

// Mixed arithmetic and comparisons, with int, float or double on either
// side: the other number becomes a FixedPoint first. (Exact matches, so the
// compiler picks these instead of converting the FixedPoint to double, which
// would make every mixed expression ambiguous; FlexReal does the same.)
template <typename T> using FixedArithmetic = std::enable_if_t<std::is_arithmetic_v<T>, int>;

#define FLEXMATH_FIXED_OPERATOR(op)                                                                \
  template <int I, int F, typename T, FixedArithmetic<T> = 0>                                      \
  inline FixedPoint<I, F> operator op(FixedPoint<I, F> a, T b) {                                   \
    return a op FixedPoint<I, F>(b);                                                               \
  }                                                                                                \
  template <int I, int F, typename T, FixedArithmetic<T> = 0>                                      \
  inline FixedPoint<I, F> operator op(T a, FixedPoint<I, F> b) {                                   \
    return FixedPoint<I, F>(a) op b;                                                               \
  }
FLEXMATH_FIXED_OPERATOR(+)
FLEXMATH_FIXED_OPERATOR(-)
FLEXMATH_FIXED_OPERATOR(*)
FLEXMATH_FIXED_OPERATOR(/)
#undef FLEXMATH_FIXED_OPERATOR

#define FLEXMATH_FIXED_COMPARISON(op)                                                              \
  template <int I, int F, typename T, FixedArithmetic<T> = 0>                                      \
  inline bool operator op(FixedPoint<I, F> a, T b) {                                               \
    return a op FixedPoint<I, F>(b);                                                               \
  }                                                                                                \
  template <int I, int F, typename T, FixedArithmetic<T> = 0>                                      \
  inline bool operator op(T a, FixedPoint<I, F> b) {                                               \
    return FixedPoint<I, F>(a) op b;                                                               \
  }
FLEXMATH_FIXED_COMPARISON(==)
FLEXMATH_FIXED_COMPARISON(!=)
FLEXMATH_FIXED_COMPARISON(<)
FLEXMATH_FIXED_COMPARISON(<=)
FLEXMATH_FIXED_COMPARISON(>)
FLEXMATH_FIXED_COMPARISON(>=)
#undef FLEXMATH_FIXED_COMPARISON

// Maths functions, named as FlexReal's are, so Cyclone's real_sqrt and friends
// map to them the same way for either: sqrt, abs and fmod in integers
// (exact); sin, cos, exp, pow and atan2 through double (correctly rounded,
// so the same everywhere, except on the DS), which is slower but rarer.
template <int I, int F> FixedPoint<I, F> flex_sqrt(FixedPoint<I, F> x) {
  using Point = FixedPoint<I, F>;
  if (x.valueFixed() <= 0)
    return Point(); // (and a negative value has no square root)
  if constexpr (I + F == 32) {
    // The root's raw value is sqrt(raw x 2^F), and raw x 2^F (under 2^47)
    // fits 64 bits
    return Point::fromFixed(detail::squareRoot32(static_cast<std::uint64_t>(x.valueFixed()) << F));
  } else {
    // raw x 2^F would need 96 bits: through double's square root instead,
    // which IEEE 754 makes exact to its 53 bits, so the same everywhere
    return Point(std::sqrt(x.value()));
  }
}
template <int I, int F> FixedPoint<I, F> flex_abs(FixedPoint<I, F> x) { return x < 0 ? -x : x; }
template <int I, int F> FixedPoint<I, F> flex_fmod(FixedPoint<I, F> x, FixedPoint<I, F> y) {
  // The remainder keeps the dividend's sign, as fmod's does
  return FixedPoint<I, F>::fromFixed(x.valueFixed() % y.valueFixed());
}
template <int I, int F> FixedPoint<I, F> flex_sin(FixedPoint<I, F> x) {
  return FixedPoint<I, F>(detail::Transcendental::sin(x.value()));
}
template <int I, int F> FixedPoint<I, F> flex_cos(FixedPoint<I, F> x) {
  return FixedPoint<I, F>(detail::Transcendental::cos(x.value()));
}
template <int I, int F> FixedPoint<I, F> flex_exp(FixedPoint<I, F> x) {
  return FixedPoint<I, F>(detail::Transcendental::exp(x.value()));
}
template <int I, int F> FixedPoint<I, F> flex_pow(FixedPoint<I, F> x, FixedPoint<I, F> y) {
  return FixedPoint<I, F>(detail::Transcendental::pow(x.value(), y.value()));
}
template <int I, int F> FixedPoint<I, F> flex_atan2(FixedPoint<I, F> y, FixedPoint<I, F> x) {
  return FixedPoint<I, F>(detail::Transcendental::atan2(y.value(), x.value()));
}

} // namespace Flex::Math
