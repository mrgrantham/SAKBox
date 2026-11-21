#include <ScratchPadd/Banner/Banner.hpp>

#include <algorithm>
#include <limits>

namespace ScratchPadd {

namespace {
constexpr double kNever = std::numeric_limits<double>::infinity();
// How far along a stretch `seconds` long, from `from`, at `now` (0 to 1)
float along(double now, double from, double seconds) {
  return static_cast<float>(std::clamp((now - from) / seconds, 0.0, 1.0));
}
} // namespace

Banner::Banner(float screenWidth) : width_(screenWidth) {}

void Banner::start(double now, std::vector<Slide> slides, Scene::Colour colour) {
  started_ = now;
  slides_.clear();
  for (Slide &slide : slides)
    slides_.push_back({std::move(slide), std::nullopt});
  colour_ = colour;
}

void Banner::start(double now, std::string title, std::string note, Scene::Colour colour) {
  start(now, {Slide{std::move(title), std::move(note)}}, colour);
}

void Banner::then(double now, Slide slide, Scene::Colour colour) {
  // (on the same band while it shows and isn't yet going; else afresh)
  const bool bandGoing = !slides_.empty() && now >= leavesAt(slides_.size() - 1) + kBandOutAfter;
  if (!showing(now) || bandGoing) {
    start(now, {std::move(slide)}, colour);
    return;
  }
  slides_.push_back({std::move(slide), std::nullopt});
}

void Banner::release(double now) {
  if (const auto i = waitingSlide())
    slides_[*i].leaves = std::max(now, entersAt(*i) + kWordsIn);
}

void Banner::hurry(double now) {
  const auto waiting = waitingSlide();
  if (!waiting) {
    stop();
    return;
  }
  // (the waiting slide alone, its band up and its words settled now)
  Shown kept = slides_[*waiting];
  slides_ = {kept};
  started_ = now - kWordsInAfter - kWordsIn;
}

void Banner::stop() {
  started_.reset();
  slides_.clear();
}

double Banner::entersAt(std::size_t i) const {
  return i == 0 ? started_.value_or(0.0) + kWordsInAfter : leavesAt(i - 1) + kWordsOut;
}

double Banner::leavesAt(std::size_t i) const {
  const Shown &shown = slides_[i];
  if (shown.leaves)
    return *shown.leaves;
  return shown.slide.waits ? kNever : entersAt(i) + kWordsIn + kHold;
}

double Banner::endsAt() const {
  if (!started_ || slides_.empty())
    return 0;
  const double last = leavesAt(slides_.size() - 1);
  return last + std::max(kWordsOut, kBandOutAfter + kBandOut);
}

std::optional<std::size_t> Banner::waitingSlide() const {
  for (std::size_t i = 0; i < slides_.size(); ++i)
    if (slides_[i].slide.waits && !slides_[i].leaves)
      return i;
  return std::nullopt;
}

bool Banner::showing(double now) const { return started_ && now < endsAt(); }

bool Banner::waiting(double now) const {
  const auto i = waitingSlide();
  return started_ && i && now >= entersAt(*i);
}

double Banner::remaining(double now) const {
  if (!started_)
    return 0;
  if (const auto i = waitingSlide())
    return std::max(0.0, entersAt(*i) + kWordsIn - now);
  return std::max(0.0, endsAt() - now);
}

std::optional<Banner::Look> Banner::at(double now) const {
  if (!showing(now))
    return std::nullopt;
  // The band: in at the start, away once the last slide's words go
  const float bandIn =
      Motion::along(Motion::Curve::EaseOut, along(now, started_.value_or(now), kBandIn));
  const double lastLeaves = leavesAt(slides_.size() - 1);
  const float bandOut =
      Motion::along(Motion::Curve::EaseIn, along(now, lastLeaves + kBandOutAfter, kBandOut));
  // The words: the slide whose turn it is (from past the right edge to the
  // middle, slowing to stop; then from the middle past the left, speeding
  // away), the next one coming in as it leaves
  std::size_t i = 0;
  while (i + 1 < slides_.size() && now >= leavesAt(i) + kWordsOut)
    ++i;
  const float in = Motion::along(Motion::Curve::EaseOut, along(now, entersAt(i), kWordsIn));
  const double leaves = leavesAt(i);
  const float out =
      leaves == kNever ? 0 : Motion::along(Motion::Curve::EaseIn, along(now, leaves, kWordsOut));
  const float middle = width_ / 2;
  const float x = middle + (1 - in) * width_ - out * width_;
  return Look{bandIn * (1 - bandOut), x, slides_[i].slide.title, slides_[i].slide.note, colour_};
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
