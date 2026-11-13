// The 3DS backend's debug mode: its settings, the watchdog and the panel
// (see ScratchPadd/Scene/3DS/Debug.hpp)
#include "Backend.hpp"

#include <ScratchPadd/Scene/3DS/Debug.hpp>

#include <3ds.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace ScratchPadd::Scene::Debug {

namespace {

// The GPU's taken this long over a frame: it's hung
constexpr std::uint64_t kHungMilliseconds = 2000;
constexpr const char *kHangReport = "sdmc:/scene-3ds-hang.txt";
// The bottom screen's console: 40 columns (39 used, so nothing wraps), 30
// rows; the watchdog's report takes its last rows
constexpr int kColumns = 39;
constexpr int kReportRow = 21;

// Prints `text` at `row` (from 1, as libctru's console counts), cut to
// the console's width, the rest of the row cleared
void printRow(int row, const char *text) {
  std::printf("\x1b[%d;1H%.*s\x1b[K", row, kColumns, text);
}

} // namespace

const char *nameOf(View view) {
  switch (view) {
  case View::normal:
    return "as usual";
  case View::normals:
    return "normals";
  case View::lighting:
    return "lighting";
  case View::vertexColours:
    return "vertex colours";
  case View::overdraw:
    return "overdraw";
  }
  return "";
}

void set(Renderer &renderer, const Settings &settings) {
  renderer.backend().debug.settings = settings;
}

const Settings &settingsOf(const Renderer &renderer) { return renderer.backend().debug.settings; }

Colour probed(const Renderer &renderer) { return renderer.backend().debug.probed; }

HangSearch hangSearch(const Renderer &renderer) {
  const auto &debug = renderer.backend().debug;
  return {debug.drawLimit, renderer.lastFrame().draws};
}

Watchdog::Watchdog(Renderer &renderer) : renderer_(renderer), thread_([this] { watch(); }) {}

Watchdog::~Watchdog() {
  stopping_ = true;
  thread_.join();
}

// Every quarter of a second: if endFrame has been drawing for two seconds,
// the GPU has hung (or a draw waited for alone has); says the draws it was
// sent last, once a hang
void Watchdog::watch() {
  auto &debug = renderer_.backend().debug;
  bool reported = false;
  while (!stopping_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (!debug.drawing.load(std::memory_order_acquire)) {
      reported = false;
      continue;
    }
    const std::uint64_t since = debug.drawingSince.load(std::memory_order_relaxed);
    if (reported || osGetTime() - since < kHungMilliseconds)
      continue;
    reported = true;
    // The last draws sent, oldest first
    const unsigned count = debug.stepCount.load(std::memory_order_acquire);
    const unsigned shown = std::min<unsigned>(count, 6);
    char lines[8][64];
    std::snprintf(lines[0], sizeof lines[0], "GPU HUNG: no frame in %llu s",
                  static_cast<unsigned long long>(kHungMilliseconds / 1000));
    std::snprintf(lines[1], sizeof lines[1], "%s",
                  debug.settings.findHang ? "Finding a hang: the last draw hung it:"
                                          : "Its frame's last draws:");
    for (unsigned i = 0; i < shown; ++i) {
      const auto &step = debug.steps[(count - shown + i) % Renderer::Backend::kSteps];
      std::snprintf(lines[2 + i], sizeof lines[2 + i], " eye %u: %s %u", step.eye, step.what,
                    step.index);
    }
    for (unsigned i = 0; i < 2 + shown; ++i)
      printRow(kReportRow + static_cast<int>(i), lines[i]);
    if (!debug.settings.findHang)
      printRow(kReportRow + 2 + static_cast<int>(shown), "(Find a hang names the draw)");
    gfxFlushBuffers(); // (the console's text to the screen, from the cache)
    if (std::FILE *file = std::fopen(kHangReport, "w")) {
      for (unsigned i = 0; i < 2 + shown; ++i)
        std::fprintf(file, "%s\n", lines[i]);
      std::fclose(file);
    }
  }
}

namespace {

// The panel's rows: the settings, in order
enum Row : int {
  kMeshes,
  kTranslucent,
  kLines,
  kWireframes,
  kOverlay,
  kFog,
  kStereo,
  kView,
  kFindHang,
  kProbe,
  kRows
};

bool *flagOf(Settings &s, int row) {
  switch (row) {
  case kMeshes:
    return &s.meshes;
  case kTranslucent:
    return &s.translucent;
  case kLines:
    return &s.lines;
  case kWireframes:
    return &s.wireframes;
  case kOverlay:
    return &s.overlay;
  case kFog:
    return &s.fog;
  case kStereo:
    return &s.stereo;
  case kFindHang:
    return &s.findHang;
  case kProbe:
    return &s.probe;
  default:
    return nullptr;
  }
}

constexpr const char *kRowNames[kRows] = {"Meshes",      "Translucent", "Lines",  "Wireframes",
                                          "Overlay",     "Fog",         "Stereo", "View",
                                          "Find a hang", "Probe"};

} // namespace

void Panel::setVisible(bool visible) {
  if (visible == visible_)
    return;
  visible_ = visible;
  cleared_ = false;
  std::printf("\x1b[2J"); // (the app redraws its own when the panel goes)
}

void Panel::change(int row, int direction) {
  Settings s = settingsOf(renderer_);
  if (row == kView) {
    const int views = static_cast<int>(View::overdraw) + 1;
    s.view = static_cast<View>((static_cast<int>(s.view) + direction + views) % views);
  } else if (bool *flag = flagOf(s, row)) {
    *flag = !*flag;
  }
  set(renderer_, s);
  cleared_ = false;
}

bool Panel::handle(std::uint32_t down, std::uint32_t held) {
  (void)held;
  if (!visible_)
    return false;
  if (down & KEY_UP)
    row_ = (row_ + kRows - 1) % kRows;
  if (down & KEY_DOWN)
    row_ = (row_ + 1) % kRows;
  if (down & (KEY_A | KEY_RIGHT))
    change(row_, 1);
  if (down & KEY_LEFT)
    change(row_, -1);
  // The circle pad moves the probe's cross
  if (settingsOf(renderer_).probe) {
    circlePosition pad;
    hidCircleRead(&pad);
    if (pad.dx * pad.dx + pad.dy * pad.dy > 20 * 20) {
      Settings s = settingsOf(renderer_);
      s.probeX = std::clamp(s.probeX + pad.dx / 24, 0, 399);
      s.probeY = std::clamp(s.probeY - pad.dy / 24, 0, 239);
      set(renderer_, s);
    }
  }
  return true;
}

void Panel::draw() {
  if (!visible_)
    return;
  if (!cleared_) {
    std::printf("\x1b[2J");
    cleared_ = true;
  }
  const Settings &s = settingsOf(renderer_);
  char text[64];
  printRow(1, "Scene debug: up/down, A, left/right");
  for (int row = 0; row < kRows; ++row) {
    const bool *flag = flagOf(const_cast<Settings &>(s), row);
    const char *value = row == kView ? nameOf(s.view) : (*flag ? "[x]" : "[ ]");
    std::snprintf(text, sizeof text, "%c %-13s %s", row == row_ ? '>' : ' ', kRowNames[row], value);
    printRow(3 + row, text);
  }
  // The probe's reading, decoded as the view has it
  if (s.probe) {
    const Colour c = probed(renderer_);
    std::snprintf(text, sizeof text, "Probe %d,%d: %3u %3u %3u (circle pad)", s.probeX, s.probeY,
                  c.r, c.g, c.b);
    printRow(14, text);
    switch (s.view) {
    case View::normals:
      std::snprintf(text, sizeof text, "  normal %+.2f %+.2f %+.2f", c.r / 127.5f - 1,
                    c.g / 127.5f - 1, c.b / 127.5f - 1);
      break;
    case View::lighting:
      std::snprintf(text, sizeof text, "  light %.2f %.2f %.2f", c.r / 255.0f, c.g / 255.0f,
                    c.b / 255.0f);
      break;
    case View::overdraw:
      std::snprintf(text, sizeof text, "  drawn %d times",
                    static_cast<int>(c.r * kOverdrawSteps / 255.0f + 0.5f));
      break;
    default:
      text[0] = '\0';
    }
    printRow(15, text);
  } else {
    printRow(14, "");
    printRow(15, "");
  }
  // How far finding a hang has got
  if (s.findHang) {
    const HangSearch search = hangSearch(renderer_);
    std::snprintf(text, sizeof text, "Finding a hang: sending %u of %u draws",
                  std::min(search.sending, search.of), search.of);
    printRow(16, text);
  } else {
    printRow(16, "");
  }
  // The last frame
  const FrameCounts counts = renderer_.lastFrame();
  const FrameTimes times = renderer_.lastFrameTimes();
  std::snprintf(text, sizeof text, "%u triangles, %u draws", counts.polygons, counts.draws);
  printRow(17, text);
  std::snprintf(text, sizeof text, "GPU %.2f ms, CPU %.2f ms", times.gpu, times.cpu);
  printRow(18, text);
}

} // namespace ScratchPadd::Scene::Debug
