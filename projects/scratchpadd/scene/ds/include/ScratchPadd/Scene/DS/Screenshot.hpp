// Screenshot.hpp - the DS's top screen saved to its SD card, as a .bmp
// (256 x 192, as the screen shows it: the 3D with the text layer over it),
// to compare with the emulator and with a photo of the screen.
//
//   std::string error;
//   const std::string path = Scene::saveScreenshot("fat:/data/my-app/screenshots/", error);
//   if (path.empty()) printf("%s", error.c_str());
//
// The display's capture unit copies one whole frame as it's shown into
// VRAM bank D (borrowed for it, and given back), so it takes a frame; then
// the file's written, which takes longer (the card is slow). Call it
// between frames.
#pragma once

#include <string>

namespace ScratchPadd::Scene {

// Saves the screen as `folder`shot_001.bmp, or the next number free, making
// the folder if it isn't there. Returns the file's path, or nothing (and
// `error` says why: no SD card, a folder or file it couldn't write).
std::string saveScreenshot(const std::string &folder, std::string &error);

} // namespace ScratchPadd::Scene
