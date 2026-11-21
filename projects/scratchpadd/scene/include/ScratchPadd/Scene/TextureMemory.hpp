// TextureMemory.hpp - a console's texture memory as its allocator hands it
// out: textures one after another, each width x height x its bytes a texel,
// none across the boundary between two slots (the DS's VRAM banks: two of
// 128 KB, 2 bytes a texel). The DS's renderer allocates with it, and any
// renderer can be held to it (Renderer::setTextureBudget), so a desktop
// test can check a console's pictures fit (an app that ran out lost its
// last pictures, silently: grey models, cards gone).
//
//   TextureMemory memory = TextureMemory::ds();
//   if (const auto at = memory.takeImage(64, 64)) ...   // 8 KB, at *at
//   memory.left();                                      // what's left
#pragma once

#include <cstdint>
#include <optional>

namespace ScratchPadd::Scene {

struct TextureMemory {
  std::uint32_t slotBytes = 128 * 1024, slots = 2, bytesPerTexel = 2;
  std::uint32_t next = 0; // (the next free byte)

  // The DS's: VRAM banks B and A, direct colour (15 bits and alpha)
  static TextureMemory ds() { return {}; }

  std::uint32_t capacity() const { return slotBytes * slots; }
  std::uint32_t used() const { return next; }
  std::uint32_t left() const { return capacity() - next; }

  // Where `bytes` go (an offset), taken; nothing if they don't fit. A piece
  // that would cross into the next slot starts at it instead
  std::optional<std::uint32_t> take(std::uint32_t bytes) {
    std::uint32_t at = next;
    if (bytes <= slotBytes && at % slotBytes + bytes > slotBytes)
      at = (at / slotBytes + 1) * slotBytes;
    if (at + bytes > capacity())
      return std::nullopt;
    next = at + bytes;
    return at;
  }
  std::optional<std::uint32_t> takeImage(int width, int height) {
    return take(static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height) *
                bytesPerTexel);
  }
};

} // namespace ScratchPadd::Scene
