// StartupSplash.hpp - words (and a small mark) shown while an app's start
// runs: faded in from black, held while the work runs, faded out to black,
// so the app's first screen can fade in after it. For a start that takes a
// few seconds (drawing every picture from its recipe takes about 5 s on a
// DS): something to watch makes the wait feel shorter.
//
//   ScratchPadd::StartupSplash splash(renderer, {.width = 256, .height = 192,
//                                                .title = "Vibe Coding by James",
//                                                .subtitle = "presents"});
//   splash.run([&] { loadEverything(); });   // returns with the screens black
//
// On the DS the fade is the screens' own brightness (a hardware register for
// each), stepped by the screen's refresh interrupt, so it's smooth while the
// work runs; the splash is drawn once, and the 3D hardware and the text layer
// keep showing it. Elsewhere the fade is drawn, a frame at a time, before and
// after the work.
#pragma once

#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/StartupSplash/Fade.hpp>

#include <functional>
#include <string>

namespace ScratchPadd {

class StartupSplash {
public:
  struct Settings {
    int width = 256, height = 192; // the screen's, in the overlay's points
    std::string title;             // the larger words
    std::string subtitle;          // under them, smaller
    bool mark = true;              // two hexagons above the words
    Scene::Colour words{232, 236, 244};
    Scene::Colour softWords{138, 148, 168};
    Scene::Colour markColour{64, 224, 208};
    double fadeSeconds = 0.5;  // each way
    double leastSeconds = 2.0; // shown at least this long, however quick the work
  };

  StartupSplash(Scene::Renderer &renderer, Settings settings);

  // Both screens black now, the first thing an app's main does, so what
  // its setup puts on them doesn't show before the splash fades in (on the
  // DS: their brightness; elsewhere nothing). run() brings them back
  static void darkenScreens();

  // Shows the splash, runs `work`, and fades it out, returning with the
  // screens black
  void run(const std::function<void()> &work);

private:
  void drawSplash(float opacity);
  Scene::Renderer &renderer_;
  Settings settings_;
};

} // namespace ScratchPadd
