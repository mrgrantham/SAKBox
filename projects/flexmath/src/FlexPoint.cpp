#include "FlexPoint.hpp"

namespace Flex::Math {

FlexPoint FlexPoint::operator/(const FlexPoint &b) const {
  return fromFixed(static_cast<FlexPointPrecision>(
      (static_cast<__int128>(valueFixed_) << kFractionalBits) / b.valueFixed_));
}

FlexPoint FlexPoint::quantized(const Precision &precision) const {
  FlexPointPrecision raw = valueFixed_;

  // Fractional precision: keep multiples of 2^-fracBits
  int drop = kFractionalBits - precision.fracBits;
  if (drop > 62) {
    drop = 62;
  }
  FlexPointPrecision unit = 1;
  if (drop > 0) {
    unit = FlexPointPrecision{1} << drop;
    if (precision.rounding == Rounding::Nearest) {
      // Halves round away from zero, matching std::round in quantize()
      raw = raw >= 0 ? ((raw + unit / 2) / unit) * unit : -(((-raw + unit / 2) / unit) * unit);
    } else if (precision.rounding == Rounding::Stochastic) {
      // Round down to the grid (masking floors in two's complement), then up
      // with probability remainder / unit, using `drop` random bits
      const FlexPointPrecision below = raw & ~(unit - 1);
      const FlexPointPrecision remainder = raw - below;
      raw = below;
      if (remainder != 0 &&
          static_cast<FlexPointPrecision>(detail::nextStochasticRoundingBits() >> (64 - drop)) <
              remainder) {
        raw += unit;
      }
    } else {
      raw = (raw / unit) * unit; // integer division truncates toward zero
    }
  }

  // Integer range; Q32.32 itself only reaches +-2^31
  if (precision.overflow == Overflow::None || precision.intBits >= 31) {
    return fromFixed(raw);
  }
  const FlexPointPrecision limit = FlexPointPrecision{1} << (precision.intBits + kFractionalBits);
  if (precision.overflow == Overflow::Saturate) {
    const FlexPointPrecision highest = limit - unit;
    return fromFixed(raw < -limit ? -limit : (raw > highest ? highest : raw));
  }
  // 2 * limit can reach 2^63, so wrap in 128 bits
  const __int128 span = static_cast<__int128>(limit) * 2;
  __int128 wrapped = (static_cast<__int128>(raw) + limit) % span;
  if (wrapped < 0) {
    wrapped += span;
  }
  return fromFixed(static_cast<FlexPointPrecision>(wrapped - limit));
}

} // namespace Flex::Math
