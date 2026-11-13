#include <ScratchPadd/Scene/3DS/Display.hpp>

#include <3ds.h>

namespace ScratchPadd::Scene {

namespace {

// Frames copied to the screen as they are: RGBA8 rendered, RGB8 shown
constexpr u32 kDisplayTransferFlags =
    GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

// Room for a frame's commands: about 200 bytes a draw, each eye's
constexpr std::size_t kCommandBufferBytes = 0x100000;

} // namespace

Display::Display() {
  C3D_Init(kCommandBufferBytes);
  gfxSet3D(true); // the right eye's frame is shown while the slider is up
  left_ = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  right_ = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetOutput(left_, GFX_TOP, GFX_LEFT, kDisplayTransferFlags);
  C3D_RenderTargetSetOutput(right_, GFX_TOP, GFX_RIGHT, kDisplayTransferFlags);
}

Display::~Display() {
  C3D_RenderTargetDelete(right_);
  C3D_RenderTargetDelete(left_);
  C3D_Fini();
}

} // namespace ScratchPadd::Scene
