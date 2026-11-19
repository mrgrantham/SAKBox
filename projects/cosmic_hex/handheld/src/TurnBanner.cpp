#include <CosmicHex/Handheld/TurnBanner.hpp>

namespace CosmicHex::Handheld {

namespace Motion = ScratchPadd::Motion;

TurnBanner::TurnBanner(float screenWidth) : width_(screenWidth) {
  // The band fades in as the words come; they hold in the middle long
  // enough to read twice, then go as the band fades
  fadeIn_ = script_.add(0.25f);
  sweepIn_ = script_.with(fadeIn_, 0.45f, 0.1f);
  hold_ = script_.after(sweepIn_, 1.3f);
  sweepOut_ = script_.after(hold_, 0.35f);
  fadeOut_ = script_.with(sweepOut_, 0.3f, 0.1f);
}

void TurnBanner::start(double now, std::string title, std::string note,
                       ScratchPadd::Scene::Colour colour) {
  started_ = now;
  title_ = std::move(title);
  note_ = std::move(note);
  colour_ = colour;
}

bool TurnBanner::showing(double now) const {
  return started_ && !script_.done(static_cast<float>(now - *started_));
}

std::optional<TurnBanner::Look> TurnBanner::at(double now) const {
  if (!showing(now))
    return std::nullopt;
  const auto elapsed = static_cast<float>(now - *started_);
  const float band = script_.progress(fadeIn_, elapsed, Motion::Curve::EaseOut) *
                     (1 - script_.progress(fadeOut_, elapsed, Motion::Curve::EaseIn));
  // From past the right edge to the middle (slowing to stop), then from the
  // middle past the left edge (speeding away)
  const float in = script_.progress(sweepIn_, elapsed, Motion::Curve::EaseOut);
  const float out = script_.progress(sweepOut_, elapsed, Motion::Curve::EaseIn);
  const float middle = width_ / 2;
  const float x = middle + (1 - in) * width_ - out * width_;
  return Look{band, x, title_, note_, colour_};
}

} // namespace CosmicHex::Handheld
