// FlexPoint.hpp - flexmath's fixed-point value type.

#pragma once

#include <FlexPrecision.hpp>
#include <compare>
#include <cstdint>
#include <fmt/format.h>
#include <ostream>
#include <type_traits>

namespace Flex::Math {

// A signed Q32.32 fixed-point number: a 64-bit integer counting units of
// 2^-32. All arithmetic is integer arithmetic, so results are bit-identical
// on every machine and compiler - use FlexPoint for state that must replay
// or stay in sync across machines.
//
// A FlexPoint always holds full Q32.32 precision. Reduced precision is
// applied when the value is read or committed, normally through a FlexGroup
// (group.view(p), group.commit(p)) or directly with quantized().
//
// Range and accuracy:
// - Values must stay within about +-2^31 (+-2,147,483,648). Exceeding it,
//   in construction or arithmetic, overflows the underlying int64_t.
// - The smallest step is 2^-32 (about 2.3e-10).
// - value() converts to double exactly while |value| < 2^21; beyond that the
//   double rounds the low bits.
//
// Arithmetic rounding:
// - + and - are exact.
// - * keeps the exact product's high bits, rounding toward negative infinity
//   in the last place (2^-32).
// - / truncates toward zero in the last place. Dividing by zero is undefined
//   behaviour (an integer division by zero), so check divisors first.
//
// Example:
//   FlexPoint a = 1.5, b = 2;            // from floating point or integers
//   FlexPoint c = a * b + 0.25;          // 3.25, exactly
//   c.quantized(Precision{.fracBits = 1}).value();   // 3.0
//   fmt::print("{} {:x}\n", c, c);       // "3.250000 0x340000000"
class FlexPoint {
public:
  // The underlying integer type: the value multiplied by 2^32.
  typedef int64_t FlexPointPrecision;
  // Twice as wide, for the intermediate results of *, / and wrap-around
  // overflow. __int128 is a GCC and Clang extension (MSVC has none; see the
  // README's Portability); __extension__ marks the use as deliberate, so
  // -pedantic-errors allows it.
  __extension__ typedef __int128 FlexPointWide;
  static constexpr int kFractionalBits = 32;

  // Zero.
  constexpr FlexPoint() {}

  // From an integer, exactly. `value` must be within +-2^31.
  template <typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
  constexpr FlexPoint(Integer value) : valueFixed_(static_cast<FlexPointPrecision>(value) * kOne) {}

  // From floating point, rounded to the nearest multiple of 2^-32.
  // `value` must be finite and within +-2^31.
  template <typename Floating,
            std::enable_if_t<std::is_floating_point<Floating>::value, bool> = true>
  FlexPoint(Floating value)
      : valueFixed_(static_cast<FlexPointPrecision>(
            std::llround(std::ldexp(static_cast<double>(value), kFractionalBits)))) {}

  // From the raw fixed-point representation (the value times 2^32), e.g. one
  // previously read with valueFixed() or loaded from a save or network.
  static constexpr FlexPoint fromFixed(FlexPointPrecision valueFixed) {
    FlexPoint point;
    point.valueFixed_ = valueFixed;
    return point;
  }

  // The value as a double (exact while |value| < 2^21).
  double value() const { return std::ldexp(static_cast<double>(valueFixed_), -kFractionalBits); }

  // The raw fixed-point representation: the value times 2^32.
  constexpr FlexPointPrecision valueFixed() const { return valueFixed_; }

  constexpr FlexPoint operator+(const FlexPoint &b) const {
    return fromFixed(valueFixed_ + b.valueFixed_);
  }
  constexpr FlexPoint operator-(const FlexPoint &b) const {
    return fromFixed(valueFixed_ - b.valueFixed_);
  }
  constexpr FlexPoint operator-() const { return fromFixed(-valueFixed_); }

  // Rounds toward negative infinity in the last place (see class comment).
  constexpr FlexPoint operator*(const FlexPoint &b) const {
    return fromFixed(static_cast<FlexPointPrecision>(
        (static_cast<FlexPointWide>(valueFixed_) * b.valueFixed_) >> kFractionalBits));
  }

  // Truncates toward zero in the last place. `b` must not be zero.
  FlexPoint operator/(const FlexPoint &b) const;

  constexpr FlexPoint &operator+=(const FlexPoint &b) { return *this = *this + b; }
  constexpr FlexPoint &operator-=(const FlexPoint &b) { return *this = *this - b; }
  constexpr FlexPoint &operator*=(const FlexPoint &b) { return *this = *this * b; }
  FlexPoint &operator/=(const FlexPoint &b) { return *this = *this / b; }

  // Exact comparison of the underlying fixed-point values. To compare at a
  // reduced precision, compare quantized values:
  //   a.quantized(p) == b.quantized(p)
  constexpr auto operator<=>(const FlexPoint &b) const = default;
  constexpr bool operator==(const FlexPoint &b) const = default;

  // This value as seen at `precision`, following the same rules as
  // quantize(): snap to 2^-fracBits with the precision's rounding, then apply
  // the integer range. Computed with integer arithmetic, so it is exact and
  // deterministic. fracBits of 32 or more leave the fraction unchanged, and
  // intBits of 31 or more leave the range unchanged (Q32.32's own limit).
  FlexPoint quantized(const Precision &precision) const;

  // Writes the value with six decimals, the same as fmt's "{}".
  friend std::ostream &operator<<(std::ostream &os, const FlexPoint &flexPoint);

private:
  static constexpr FlexPointPrecision kOne = FlexPointPrecision{1} << kFractionalBits;

  FlexPointPrecision valueFixed_{0};
};

} // namespace Flex::Math

// fmt support: "{}" or "{:f}" prints the value with six decimals ("3.250000"),
// "{:x}" prints the raw fixed-point bits in hex ("0x340000000").
template <> struct fmt::formatter<Flex::Math::FlexPoint> {
  char underlying_formatter_type = 'f';

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 'f' || *it == 'x'))
      underlying_formatter_type = *it++;
    if (it != end && *it != '}')
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext>
  auto format(const Flex::Math::FlexPoint &p, FormatContext &ctx) const -> decltype(ctx.out()) {
    return underlying_formatter_type == 'f' ? fmt::format_to(ctx.out(), "{:.6f}", p.value())
                                            : fmt::format_to(ctx.out(), "{:#x}", p.valueFixed());
  }
};

namespace Flex::Math {

inline std::ostream &operator<<(std::ostream &os, const FlexPoint &flexPoint) {
  return os << fmt::format("{}", flexPoint);
}

} // namespace Flex::Math
