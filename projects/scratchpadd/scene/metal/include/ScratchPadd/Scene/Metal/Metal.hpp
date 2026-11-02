// Metal.hpp - the Metal backend's device and frames, for desktop code that
// draws with Metal itself: debug_ui's ImGui. Apps don't need it.
#pragma once

#include <ScratchPadd/Scene/Desktop/Display.hpp>

namespace MTL {
class CommandBuffer;
class CommandQueue;
class Device;
class RenderPassDescriptor;
} // namespace MTL

namespace ScratchPadd::Scene::Metal {

// The Mac's Metal device and the one queue everything is drawn through
// (the renderer, ShaderPanels, ImGui), so their work runs in the order it's
// committed; null if the Mac has no Metal device
MTL::Device *device();
MTL::CommandQueue *queue();

// While Display::present runs its last drawings: the frame's command
// buffer, and a pass drawing over the finished frame (its colour loaded,
// one sample, no depth). Null at any other time.
struct Frame {
  MTL::CommandBuffer *commands;
  MTL::RenderPassDescriptor *pass;
};
const Frame *currentFrame(const Display &display);
// A pass like the frame's, at the display's current size, for what needs
// its formats before a frame begins (ImGui's NewFrame)
MTL::RenderPassDescriptor *framePass(const Display &display);

} // namespace ScratchPadd::Scene::Metal
