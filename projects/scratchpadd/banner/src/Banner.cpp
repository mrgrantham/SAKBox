#include <ScratchPadd/Banner/Banner.hpp>

#include <algorithm>

namespace ScratchPadd {

Banner::Banner(float screenWidth) : width_(screenWidth) {
  // The band fades in as the words come; they hold in the middle long
  // enough to read twice, then go as the band fades
  fadeIn_ = script_.add(0.25f);
  sweepIn_ = script_.with(fadeIn_, 0.45f, 0.1f);
  hold_ = script_.after(sweepIn_, 1.3f);
  sweepOut_ = script_.after(hold_, 0.35f);
  fadeOut_ = script_.with(sweepOut_, 0.3f, 0.1f);
}

void Banner::start(double now, std::string title, std::string note, Scene::Colour colour) {
  started_ = now;
  title_ = std::move(title);
  note_ = std::move(note);
  colour_ = colour;
}

double Banner::remaining(double now) const {
  return started_ ? std::max(0.0, *started_ + script_.seconds() - now) : 0.0;
}

bool Banner::showing(double now) const {
  return started_ && !script_.done(static_cast<float>(now - *started_));
}

std::optional<Banner::Look> Banner::at(double now) const {
  if (!started_ || !showing(now))
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

void Banner::draw(Scene::Overlay &overlay, const Look &look, const Style &style) {
  const float k = style.scale;
  const float y = style.screenHeight * 0.42f, h = 32 * k;
  const auto alpha = [&](float most) { return static_cast<std::uint8_t>(most * look.bandAlpha); };
  const auto with = [](Scene::Colour c, std::uint8_t a) { return Scene::Colour{c.r, c.g, c.b, a}; };
  overlay.rectangle(0, y, style.screenWidth, h, with(style.back, alpha(225)));
  overlay.rectangle(0, y, style.screenWidth, h, with(look.colour, alpha(80)));
  overlay.rectangle(0, y, style.screenWidth, 1, with(style.accent, alpha(255)));
  overlay.rectangle(0, y + h - 1, style.screenWidth, 1, with(style.accent, alpha(255)));
  overlay.textCentred(look.wordsX, y + 11 * k, look.title, {240, 244, 250}, style.titleScale);
  overlay.textCentred(look.wordsX, y + 24 * k, look.note, {170, 180, 198});
}

} // namespace ScratchPadd
