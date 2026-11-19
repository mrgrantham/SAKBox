// ConsoleHardware.hpp - the few calls that differ by console, beside the
// settings in PlatformProfile.hpp (round 92: they were #ifs through the
// app): whether the app should keep running, waiting for the screen's
// refresh, and putting the bottom screen's pixels on it.
//
//   BottomScreen bottom;                         // set up once
//   bottom.present(width, height, colourAt);     // when the picture changes
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <cstddef>
#include <cstdint>

#if defined(__NDS__)
#include <nds.h>
#else
#include <3ds.h>
#endif

namespace ConsoleHardware {

// The console's graphics, set up for as long as one lives (the 3DS's gfx
// library; the DS needs nothing): made first in main, before the screens
class Graphics {
public:
  Graphics() {
#if !defined(__NDS__)
    gfxInitDefault();
#endif
  }
  ~Graphics() {
#if !defined(__NDS__)
    gfxExit();
#endif
  }
  Graphics(const Graphics &) = delete;
  Graphics &operator=(const Graphics &) = delete;
};

// False once the system asks the app to end (the 3DS's HOME menu, the DS's
// power management)
inline bool keepRunning() {
#if defined(__NDS__)
  return pmMainLoop();
#else
  return aptMainLoop();
#endif
}

// Waits for the screen's refresh without drawing: on the DS the 3D hardware
// shows the last frame again by itself (the top screen held still). Not
// called on the 3DS, whose frames must be sent (PlatformProfile::
// holdStillFrames)
inline void waitForTheRefresh() {
#if defined(__NDS__)
  swiWaitForVBlank();
#endif
}

// The bottom screen as one picture, written straight to the hardware: the
// DS's 16-bit bitmap background (VRAM bank C: the scene layer has B, D and
// E), or the 3DS's framebuffer (24-bit, turned a quarter: its rows are the
// screen's columns)
class BottomScreen {
public:
  BottomScreen() {
#if defined(__NDS__)
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    pixels_ = bgGetGfxPtr(bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0));
#else
    gfxSetDoubleBuffering(GFX_BOTTOM, false);
#endif
  }

  // A picture as it is, `pixels` in rows of `width`: on the DS a tight
  // copy, each pixel read as one word and two written at a time (round
  // 105: per pixel through present() below it took up to 30 ms)
  void present(int width, int height, const ScratchPadd::Scene::Colour *pixels) {
#if defined(__NDS__)
    static_assert(sizeof(ScratchPadd::Scene::Colour) == 4);
    // (a vector's data is aligned for any type: its pixels as words, which
    // may alias them)
    using Word [[gnu::may_alias]] = std::uint32_t;
    const auto *from =
        static_cast<const Word *>(__builtin_assume_aligned(static_cast<const void *>(pixels), 4));
    // Red, green and blue bytes (little-endian: red lowest) to 15 bits,
    // bit 15 set
    const auto fifteen = [](std::uint32_t c) {
      return ((c >> 3) & 0x1Fu) | ((c >> 6) & 0x3E0u) | ((c >> 9) & 0x7C00u) | 0x8000u;
    };
    for (int y = 0; y < height; ++y) {
      auto *to = reinterpret_cast<volatile std::uint32_t *>(pixels_ + y * 256);
      const Word *row = from + static_cast<std::ptrdiff_t>(y) * width;
      for (int x = 0; x + 1 < width; x += 2)
        to[x / 2] = fifteen(row[x]) | (fifteen(row[x + 1]) << 16);
    }
#else
    present(width, height, [pixels](std::size_t i) { return pixels[i]; });
#endif
  }

  // The picture, `colourAt(i)` the colour of pixel i (rows of `width`)
  template <typename ColourAt> void present(int width, int height, ColourAt &&colourAt) {
#if defined(__NDS__)
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const ScratchPadd::Scene::Colour c = colourAt(static_cast<std::size_t>(y * width + x));
        // (256 a row, the bitmap's width; 15-bit colours with bit 15 set)
        pixels_[y * 256 + x] = static_cast<u16>(RGB15(c.r >> 3, c.g >> 3, c.b >> 3) | BIT(15));
      }
#else
    u16 w = 0, h = 0;
    std::uint8_t *frame = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const ScratchPadd::Scene::Colour c = colourAt(static_cast<std::size_t>(y * width + x));
        std::uint8_t *at = frame + (static_cast<std::size_t>(x) * static_cast<std::size_t>(height) +
                                    static_cast<std::size_t>(height - 1 - y)) *
                                       3;
        at[0] = c.b;
        at[1] = c.g;
        at[2] = c.r;
      }
    gfxFlushBuffers();
#endif
  }

private:
#if defined(__NDS__)
  u16 *pixels_ = nullptr;
#endif
};

} // namespace ConsoleHardware
