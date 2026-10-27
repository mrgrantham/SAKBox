// FlexReal.hpp - a floating-point type whose every operation snaps to an
// adjustable precision.

#pragma once

#include <FlexPrecision.hpp>
#include <cmath>
#include <type_traits>

namespace Flex::Math {

// The precision FlexReal values on the current thread snap to. Starts as
// kFullPrecision on every thread. Usually set for a scope with
// ScopedActivePrecision or FlexGroup::activate(), but may be changed directly
// (e.g. by a debug key that steps precision up and down).
inline Precision &activePrecision() {
  thread_local Precision precision;
  return precision;
}

// Sets the active precision for the current scope and restores the previous
// one on exit. Scopes nest.
//   {
//     ScopedActivePrecision coarse(Precision{.fracBits = 4});
//     FlexReal x = 0.1;   // 0.0625
//   }
class ScopedActivePrecision {
public:
  explicit ScopedActivePrecision(const Precision &precision) : saved_(activePrecision()) {
    activePrecision() = precision;
  }
  ~ScopedActivePrecision() { activePrecision() = saved_; }
  ScopedActivePrecision(const ScopedActivePrecision &) = delete;
  ScopedActivePrecision &operator=(const ScopedActivePrecision &) = delete;

private:
  Precision saved_;
};

// A double that snaps every value it holds to the active precision: values
// it is constructed from, and the result of every arithmetic operation. This
// implements the PerOperation policy - lowering precision degrades an entire
// computation, not just its inputs and outputs.
//
// It is designed as a drop-in replacement for a `real` typedef: Cyclone's
// flex build (//projects/cyclone:cyclone_flex) compiles its whole engine
// with `typedef Flex::Math::FlexReal real;`.
//
// Behaviour to know about:
// - Mixed arithmetic (FlexReal with int, float or double, on either side)
//   produces a FlexReal and snaps.
// - It converts implicitly to double, so it can be passed to functions taking
//   double or float. Passing it through C varargs (printf and friends) does
//   not convert it; cast with (double) or call value().
// - It emulates precision loss only. Values keep double's range; use
//   Precision::overflow on a group for range effects.
// - flex_sin and the other functions below compute in double with the
//   platform's math library and then snap. Results can differ slightly
//   between platforms, so FlexReal is not a determinism guarantee; use
//   FlexPoint for state that must match across machines.
// - The default constructor leaves the value uninitialised, like double.
//   This keeps FlexReal trivial, so it can be a member of a union.
class FlexReal {
public:
  FlexReal() = default;
  FlexReal(double value) : value_(quantize(value, activePrecision())) {}

  operator double() const { return value_; }
  double value() const { return value_; }

  FlexReal operator-() const { return FlexReal(-value_); }
  FlexReal &operator+=(FlexReal b) { return *this = FlexReal(value_ + b.value_); }
  FlexReal &operator-=(FlexReal b) { return *this = FlexReal(value_ - b.value_); }
  FlexReal &operator*=(FlexReal b) { return *this = FlexReal(value_ * b.value_); }
  FlexReal &operator/=(FlexReal b) { return *this = FlexReal(value_ / b.value_); }

private:
  double value_;
};

// Arithmetic and comparison operators.
//
// Each operator has three overloads: (FlexReal, FlexReal), (FlexReal, T) and
// (T, FlexReal) for any arithmetic T. The template overloads match a mixed
// expression like `x * 2.0` exactly, so the compiler picks them instead of
// converting x to double and using the built-in operator - which would
// otherwise make every mixed expression ambiguous.
template <typename T> using FlexArithmetic = std::enable_if_t<std::is_arithmetic_v<T>, int>;

#define FLEXMATH_REAL_OPERATOR(op)                                                                 \
  inline FlexReal operator op(FlexReal a, FlexReal b) { return FlexReal(a.value() op b.value()); } \
  template <typename T, FlexArithmetic<T> = 0> inline FlexReal operator op(FlexReal a, T b) {      \
    return FlexReal(a.value() op static_cast<double>(b));                                          \
  }                                                                                                \
  template <typename T, FlexArithmetic<T> = 0> inline FlexReal operator op(T a, FlexReal b) {      \
    return FlexReal(static_cast<double>(a) op b.value());                                          \
  }
FLEXMATH_REAL_OPERATOR(+)
FLEXMATH_REAL_OPERATOR(-)
FLEXMATH_REAL_OPERATOR(*)
FLEXMATH_REAL_OPERATOR(/)
#undef FLEXMATH_REAL_OPERATOR

#define FLEXMATH_REAL_COMPARISON(op)                                                               \
  inline bool operator op(FlexReal a, FlexReal b) { return a.value() op b.value(); }               \
  template <typename T, FlexArithmetic<T> = 0> inline bool operator op(FlexReal a, T b) {          \
    return a.value() op static_cast<double>(b);                                                    \
  }                                                                                                \
  template <typename T, FlexArithmetic<T> = 0> inline bool operator op(T a, FlexReal b) {          \
    return static_cast<double>(a) op b.value();                                                    \
  }
FLEXMATH_REAL_COMPARISON(==)
FLEXMATH_REAL_COMPARISON(!=)
FLEXMATH_REAL_COMPARISON(<)
FLEXMATH_REAL_COMPARISON(<=)
FLEXMATH_REAL_COMPARISON(>)
FLEXMATH_REAL_COMPARISON(>=)
#undef FLEXMATH_REAL_COMPARISON

// Math functions: computed in double, then snapped to the active precision.
// Named to match Cyclone's real_sqrt / real_sin / ... macros.
inline FlexReal flex_sqrt(FlexReal x) { return FlexReal(std::sqrt(x.value())); }
inline FlexReal flex_abs(FlexReal x) { return FlexReal(std::fabs(x.value())); }
inline FlexReal flex_sin(FlexReal x) { return FlexReal(std::sin(x.value())); }
inline FlexReal flex_cos(FlexReal x) { return FlexReal(std::cos(x.value())); }
inline FlexReal flex_exp(FlexReal x) { return FlexReal(std::exp(x.value())); }
inline FlexReal flex_pow(FlexReal x, FlexReal y) {
  return FlexReal(std::pow(x.value(), y.value()));
}
inline FlexReal flex_fmod(FlexReal x, FlexReal y) {
  return FlexReal(std::fmod(x.value(), y.value()));
}
inline FlexReal flex_atan2(FlexReal y, FlexReal x) {
  return FlexReal(std::atan2(y.value(), x.value()));
}

} // namespace Flex::Math
