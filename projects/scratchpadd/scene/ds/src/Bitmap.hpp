// Bitmap.hpp - a picture as a .bmp file's bytes: what the DS's screenshots
// are saved as (Screenshot.hpp), since a BMP needs no compression code. Its
// pixels come as the DS has them: 15-bit colours, red in the low 5 bits,
// then green, then blue (the top bit unused), rows from the top.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ScratchPadd::Scene::DisplayList {

// The DS's 5 bits a channel as 8 (31 is 255), as hardware does it: the top
// bits repeated in the bottom
constexpr std::uint8_t channel8(unsigned five) {
  return static_cast<std::uint8_t>((five << 3) | (five >> 2));
}

// A 24-bit BMP of `pixels` (width x height, rows from the top)
inline std::vector<std::uint8_t> bmpFromRgb15(std::span<const std::uint16_t> pixels, int width,
                                              int height) {
  const int rowBytes = (width * 3 + 3) / 4 * 4; // (each row a multiple of 4 bytes)
  const int imageBytes = rowBytes * height;
  constexpr int kHeaderBytes = 14 + 40;
  std::vector<std::uint8_t> out(static_cast<std::size_t>(kHeaderBytes + imageBytes), 0);
  const auto put32 = [&](std::size_t at, std::uint32_t value) {
    for (int i = 0; i < 4; ++i)
      out[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (8 * i));
  };
  // The file header, then the bitmap's (BITMAPINFOHEADER)
  out[0] = 'B';
  out[1] = 'M';
  put32(2, static_cast<std::uint32_t>(out.size()));
  put32(10, kHeaderBytes);
  put32(14, 40);
  put32(18, static_cast<std::uint32_t>(width));
  put32(22, static_cast<std::uint32_t>(height)); // (positive: rows from the bottom)
  out[26] = 1;                                   // planes
  out[28] = 24;                                  // bits a pixel
  put32(34, static_cast<std::uint32_t>(imageBytes));
  // The rows, bottom first, each pixel blue, green, red
  for (int y = 0; y < height; ++y) {
    std::uint8_t *row =
        &out[static_cast<std::size_t>(kHeaderBytes) +
             static_cast<std::size_t>(height - 1 - y) * static_cast<std::size_t>(rowBytes)];
    for (int x = 0; x < width; ++x) {
      const std::uint16_t c = pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                     static_cast<std::size_t>(x)];
      row[x * 3 + 0] = channel8((c >> 10) & 31);
      row[x * 3 + 1] = channel8((c >> 5) & 31);
      row[x * 3 + 2] = channel8(c & 31);
    }
  }
  return out;
}

} // namespace ScratchPadd::Scene::DisplayList
