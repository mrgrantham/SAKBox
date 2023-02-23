#pragma once

#include <FlexController.hpp>
#include <compare>
#include <cstdint>
#include <fmt/format.h>
#include <limits>
#include <type_traits>

namespace Flex::Math {

class FlexPoint {
public:
  typedef int64_t FlexPointPrecision;
  FlexPoint() {}

  template <typename Integer,
            std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
  FlexPoint(Integer value) {
    // TODO: Conveter int to fixed point based on flextype
    flexGroupID_ = FlexController::getInstance()->registerFlexPoint(this);
  }

  ~FlexPoint() {
    FlexController::getInstance()
        ->getFlexGroup(flexGroupID_)
        ->deregisterFlexPoint(this);
  }

  template <
      typename Floating,
      std::enable_if_t<std::is_floating_point<Floating>::value, bool> = true>
  FlexPoint(Floating value) {
    // TODO: Conveter floating point to fixed point based on flextype
    flexGroupID_ = FlexController::getInstance()->registerFlexPoint(this);
  }

  void setFractionalComponent(int32_t precision) {
    fractionalComponent_ = precision;
  }

  FlexPoint operator-(const FlexPoint &value) const {
    FlexPoint newValue;
    newValue.setFractionalComponent(this->getFractionalComponent());
    newValue = this->valueFixed_ - value.valueFixed_;
    return newValue;
  }

  FlexPoint operator+(const FlexPoint &value) const {
    FlexPoint newValue;
    newValue.setFractionalComponent(this->getFractionalComponent());
    newValue = this->valueFixed_ + value.valueFixed_;
    return newValue;
  }

  FlexPoint operator*(const FlexPoint &value) const {
    FlexPoint newValue;
    newValue.setFractionalComponent(this->getFractionalComponent());
    newValue = this->valueFixed_ * value.valueFixed_;
    return newValue;
  }

  FlexPoint &operator=(const FlexPoint &b);
  FlexPoint &operator=(const double &b);
  FlexPoint &operator+=(const FlexPoint &b);
  FlexPoint &operator-=(const FlexPoint &b);
  FlexPoint &operator*=(const FlexPoint &b);

  inline double convertFixedToDouble(double fixedValue) const {
    return static_cast<double>(valueFixed_) /
           static_cast<double>(1 << fractionalComponent_);
  }

  inline FlexPointPrecision convertDoubleToFixed(double fixedValue) const {
    return static_cast<FlexPointPrecision>(valueFixed_ *
                                           (1 << fractionalComponent_));
  }

  double value() const { return convertFixedToDouble(valueFixed_); }

  FlexPointPrecision valueFixed() const { return valueFixed_; }

  auto operator<=>(const FlexPoint &value) const {
    return (*this - value).value();
  }
  auto operator==(const FlexPoint &value) const {
    return this->value() == value.value();
  }

  int16_t getFractionalComponent() const { return fractionalComponent_; }

  friend std::ostream &operator<<(std::ostream &os,
                                  const FlexPoint &flexPoint) {
    os << fmt::format("(v{:.6f} p{:d})", flexPoint.value(),
                      flexPoint.getFractionalComponent());
    return os;
  }

private:
  FlexGroupID flexGroupID_;
  int16_t fractionalComponent_{8};
  FlexPointPrecision valueFixed_{0};
};

} // namespace Flex::Math

template <> struct fmt::formatter<Flex::Math::FlexPoint> {
  char underlying_formatter_type = 'f';
  // // Parses format specifications of the form ['f' | 'e'].
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    // [ctx.begin(), ctx.end()) is a character range that contains a part of
    // the format string starting from the format specifications to be parsed,
    // e.g. in
    //
    //   fmt::format("{:f} - point of interest", point{1, 2});
    //
    // the range will contain "f} - point of interest". The formatter should
    // parse specifiers until '}' or the end of the range. In this example
    // the formatter should parse the 'f' specifier and return an iterator
    // pointing to '}'.

    // Parse the presentation format and store it in the formatter:
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 'f' || *it == 'x'))
      underlying_formatter_type = *it++;

    // Check if reached the end of the range:
    if (it != end && *it != '}')
      throw format_error("invalid format");

    // Return an iterator past the end of the parsed range:
    return it;
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const Flex::Math::FlexPoint &p, FormatContext &ctx)
      -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    return underlying_formatter_type == 'f'
               ? format_to(ctx.out(), "(v{:.6f} f{:d})", p.value(),
                           p.getFractionalComponent())
               : format_to(ctx.out(), "(v{:x} f{:d})", p.valueFixed(),
                           p.getFractionalComponent());
  }
};