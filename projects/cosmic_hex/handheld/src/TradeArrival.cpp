#include <CosmicHex/Handheld/TradeArrival.hpp>

namespace CosmicHex::Handheld {

bool TradeArrival::saw(bool answering, double now) {
  const bool arrived = answering && !answering_;
  answering_ = answering;
  if (arrived)
    since_.start(now);
  else if (!answering)
    since_.stop();
  return arrived;
}

float TradeArrival::down(double now) const {
  if (!since_.on())
    return 1;
  return ScratchPadd::Motion::along(
      ScratchPadd::Motion::Curve::EaseOut,
      static_cast<float>((since_.seconds(now) - kStartsAfter) / kSeconds));
}

bool TradeArrival::moving(double now) const {
  return since_.on() && since_.seconds(now) < kStartsAfter + kSeconds;
}

} // namespace CosmicHex::Handheld
