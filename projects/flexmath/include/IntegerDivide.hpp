// IntegerDivide.hpp - whole-number division, quotient and remainder, the
// same on every platform, each done the fastest way the platform has
//
// The DS's ARM9 has no divide instruction: `a / b` there is a library call,
// a few dozen cycles for 32 bits and a few hundred for 64. Its hardware
// divider does either in about 20 to 35 (IntegerDivideNds.hpp, chosen here
// when building for the DS). Everywhere else these are C++'s / and %.
//
// Both round towards zero, as C++ does, so results are the same everywhere
// (a drawing hashes the same on a DS as on a Mac). Dividing by zero isn't
// allowed (as with /). A division by a constant needs none of this: the
// compiler makes it a multiply.
//
//   const auto [quotient, remainder] = Flex::Math::divide64(along, height);
#pragma once

#include <cstdint>

namespace Flex::Math {

template <typename Int> struct Divided {
  Int quotient;
  Int remainder;
};

} // namespace Flex::Math

#if defined(__NDS__)
#include <IntegerDivideNds.hpp>
#else
namespace Flex::Math {

inline Divided<std::int32_t> divide32(std::int32_t numerator, std::int32_t denominator) {
  return {numerator / denominator, numerator % denominator};
}
inline Divided<std::int64_t> divide64(std::int64_t numerator, std::int64_t denominator) {
  return {numerator / denominator, numerator % denominator};
}

} // namespace Flex::Math
#endif
