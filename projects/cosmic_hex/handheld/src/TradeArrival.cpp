#include <CosmicHex/Handheld/TradeArrival.hpp>

#include <limits>

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

void TradeArrival::waitForBanner() {
  if (since_.on()) // (an answer arriving: timed from no time yet, so not down)
    since_.start(std::numeric_limits<double>::infinity());
}

void TradeArrival::after(double bannerAt) {
  if (since_.on())
    since_.start(bannerAt);
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
