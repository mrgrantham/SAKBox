// The DS's display capture (GBATEK, "DS Display Capture"): DISPCAPCNT set
// to capture source A, the composited screen (the 2D layers and the 3D, as
// shown), 256 x 192 into VRAM bank D, then the bank read as LCD memory.
#include <ScratchPadd/Scene/DS/Screenshot.hpp>

#include "../src/Bitmap.hpp" // (staged beside it: see ../BUILD)

#include <nds.h>

#include <fat.h>
#include <sys/stat.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ScratchPadd::Scene {

namespace {

constexpr int kWidth = 256, kHeight = 192;

// The screen, a frame as it's shown, 15-bit colours rows from the top
std::vector<std::uint16_t> captureScreen() {
  const std::uint8_t bankWas = VRAM_D_CR; // (given back after)
  vramSetBankD(VRAM_D_LCD);
  REG_DISPCAPCNT = DCAP_ENABLE | DCAP_MODE(DCAP_MODE_A) | DCAP_SRC_A(DCAP_SRC_A_COMPOSITED) |
                   DCAP_SIZE(DCAP_SIZE_256x192) | DCAP_BANK(DCAP_BANK_VRAM_D);
  // It starts with the next frame and clears its enable bit when done
  while (REG_DISPCAPCNT & DCAP_ENABLE) {
  }
  std::vector<std::uint16_t> pixels(VRAM_D, VRAM_D + kWidth * kHeight);
  VRAM_D_CR = bankWas;
  return pixels;
}

// Makes each folder along `path` (ending in /) that isn't there
bool makeFolders(const std::string &path) {
  for (std::size_t slash = path.find('/', path.find(':') + 2); slash != std::string::npos;
       slash = path.find('/', slash + 1)) {
    const std::string folder = path.substr(0, slash);
    if (mkdir(folder.c_str(), 0777) != 0 && errno != EEXIST)
      return false;
  }
  return true;
}

} // namespace

std::string saveScreenshot(const std::string &folder, std::string &error) {
  // (libfat: the flashcart's card, through its DLDI driver; started once)
  static const bool started = fatInitDefault();
  if (!started) {
    error = "No SD card (or no DLDI driver)";
    return {};
  }
  const std::vector<std::uint16_t> pixels = captureScreen();
  if (!makeFolders(folder)) {
    error = "Couldn't make " + folder;
    return {};
  }
  // The next number free
  std::string path;
  for (int n = 1; n < 1000; ++n) {
    char name[32];
    std::snprintf(name, sizeof name, "shot_%03d.bmp", n);
    path = folder + name;
    struct stat existing;
    if (stat(path.c_str(), &existing) != 0)
      break;
  }
  const std::vector<std::uint8_t> bmp = DisplayList::bmpFromRgb15(pixels, kWidth, kHeight);
  std::FILE *file = std::fopen(path.c_str(), "wb");
  if (!file || std::fwrite(bmp.data(), 1, bmp.size(), file) != bmp.size()) {
    if (file)
      std::fclose(file);
    error = "Couldn't write " + path;
    return {};
  }
  std::fclose(file);
  return path;
}

} // namespace ScratchPadd::Scene
