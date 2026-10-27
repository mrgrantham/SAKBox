// FlexPoint.hpp - flexmath's fixed-point value types, FlexPoint (Q32.32) and
// FlexPoint16 (Q16.16) (see FixedPoint.hpp), with printing through fmt and
// streams.
//
// Example:
//   FlexPoint a = 1.5, b = 2;            // from floating point or integers
//   FlexPoint c = a * b + 0.25;          // 3.25, exactly
//   c.quantized(Precision{.fracBits = 1}).value();   // 3.0
//   fmt::print("{} {:x}\n", c, c);       // "3.250000 0x340000000"

#pragma once

#include <FixedPoint.hpp>
#include <fmt/format.h>
#include <ostream>

// fmt support: "{}" or "{:f}" prints the value with six decimals ("3.250000"),
// "{:x}" prints the raw fixed-point bits in hex ("0x340000000").
template <int I, int F> struct fmt::formatter<Flex::Math::FixedPoint<I, F>> {
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
  auto format(const Flex::Math::FixedPoint<I, F> &p, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    return underlying_formatter_type == 'f' ? fmt::format_to(ctx.out(), "{:.6f}", p.value())
                                            : fmt::format_to(ctx.out(), "{:#x}", p.valueFixed());
  }
};

namespace Flex::Math {

// Writes the value with six decimals, the same as fmt's "{}".
template <int I, int F> std::ostream &operator<<(std::ostream &os, const FixedPoint<I, F> &point) {
  return os << fmt::format("{}", point);
}

} // namespace Flex::Math
