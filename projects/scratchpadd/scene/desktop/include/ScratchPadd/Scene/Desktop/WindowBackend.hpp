// WindowBackend.hpp - what a desktop backend does for a Display: each
// backend (OpenGL, Metal) defines these, and the Display calls them. For
// backends only; apps use Display.
#pragma once

#include <ScratchPadd/Scene/Desktop/Display.hpp>

#include <string>

namespace ScratchPadd::Scene::WindowBackend {

// The backend's name, for messages ("OpenGL 3.3", "Metal")
extern const std::string kName;
// GLFW's window hints for the backend, before a window is made
void hint();
// Sets up a new window to draw in (display.glfw()), keeping what it needs
// with Display::setSurface; returns "" or why it can't
std::string attach(Display &display);
// Shows the frame, after the Display's last drawings
void show(Display &display);

} // namespace ScratchPadd::Scene::WindowBackend
