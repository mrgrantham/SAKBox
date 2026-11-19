#include <ScratchPadd/StartupSplash/StartupSplash.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#if defined(__NDS__)
#include <nds.h>
#endif

namespace ScratchPadd {

namespace {
using Clock = std::chrono::steady_clock;
constexpr double kRefreshesASecond = 60;

double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

#if defined(__NDS__)
// The fade in, stepped by the refresh interrupt while the work runs: the
// refreshes so far and how many it takes
volatile int fadeFrame = 0;
int fadeFrames = 1;
void stepFadeIn() {
  if (fadeFrame < fadeFrames)
    ++fadeFrame;
  const int level = brightnessAt(fadeFrame, fadeFrames, true);
  setBrightness(1, level); // (the top screen; the bottom stays black)
}
#endif
} // namespace

void StartupSplash::darkenScreens() {
#if defined(__NDS__)
  setBrightness(3, -16);
#endif
}

StartupSplash::StartupSplash(Scene::Renderer &renderer, Settings settings)
    : renderer_(renderer), settings_(std::move(settings)) {}

void StartupSplash::drawSplash(float opacity) {
  // A blank scene: the background black, nothing in 3D
  renderer_.beginFrame(Scene::Camera{}, Scene::Environment{.background = {0, 0, 0}});
  Scene::Overlay &overlay = renderer_.overlay();
  const auto faded = [opacity](Scene::Colour c) {
    c.r = static_cast<std::uint8_t>(std::lround(c.r * opacity));
    c.g = static_cast<std::uint8_t>(std::lround(c.g * opacity));
    c.b = static_cast<std::uint8_t>(std::lround(c.b * opacity));
    return c;
  };
  const float k = static_cast<float>(settings_.width) / 256;
  const float middle = static_cast<float>(settings_.width) / 2;
  float y = static_cast<float>(settings_.height) / 2 - 6 * k;
  if (settings_.mark) {
    // Two hexagons, one in the other, pointy-topped
    const float cy = y - 34 * k;
    for (const float radius : {22 * k, 13 * k}) {
      for (int i = 0; i < 6; ++i) {
        const float a = 3.14159265f / 3 * static_cast<float>(i) - 3.14159265f / 2;
        const float b = a + 3.14159265f / 3;
        overlay.line(middle + radius * std::cos(a), cy + radius * std::sin(a),
                     middle + radius * std::cos(b), cy + radius * std::sin(b),
                     faded(settings_.markColour), radius > 15 * k ? 2 : 1);
      }
    }
    y += 12 * k;
  }
  overlay.textCentred(middle, y, settings_.title, faded(settings_.words), 1.5f * k);
  if (!settings_.subtitle.empty())
    overlay.textCentred(middle, y + 20 * k, settings_.subtitle, faded(settings_.softWords), k);
  renderer_.endFrame();
}

void StartupSplash::run(const std::function<void()> &work) {
  const Clock::time_point started = Clock::now();
  const int frames = std::max(1, static_cast<int>(settings_.fadeSeconds * kRefreshesASecond));
#if defined(__NDS__)
  // Black, the splash drawn once (the hardware keeps showing it), and the
  // top screen brought up by the refresh interrupt while the work runs
  setBrightness(3, -16);
  drawSplash(1);
  fadeFrame = 0;
  fadeFrames = frames;
  irqSet(IRQ_VBLANK, stepFadeIn);
  work();
  while (secondsSince(started) < settings_.leastSeconds || fadeFrame < fadeFrames)
    swiWaitForVBlank();
  irqSet(IRQ_VBLANK, nullptr);
  for (int frame = 0; frame <= frames; ++frame) {
    setBrightness(1, brightnessAt(frame, frames, false));
    swiWaitForVBlank();
  }
  // (black drawn, so the brightness can go back to normal unseen)
  renderer_.beginFrame(Scene::Camera{}, Scene::Environment{.background = {0, 0, 0}});
  renderer_.endFrame();
  setBrightness(3, 0);
#else
  // A frame at a time: in, held while the work runs, out
  for (int frame = 0; frame <= frames; ++frame)
    drawSplash(static_cast<float>(frame) / static_cast<float>(frames));
  work();
  while (secondsSince(started) < settings_.leastSeconds)
    drawSplash(1);
  for (int frame = frames; frame >= 0; --frame)
    drawSplash(static_cast<float>(frame) / static_cast<float>(frames));
#endif
}

} // namespace ScratchPadd
