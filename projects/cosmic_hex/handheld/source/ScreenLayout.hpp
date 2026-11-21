// ScreenLayout.hpp - where the app draws, by role, in one place. Two
// areas:
//
//   the stage        the 3D scene: the title, the setup's board, the
//                    game's board and what shows over it (the banners, the
//                    HUD, the close-ups); the DS's and 3DS's top screen
//   the touch panel  what's touched: the menus, the settings, the flat map
//                    and its buttons; their bottom screen
//
// The app's parts ask for these rather than for a console's screens, so a
// one-screen version can place both on its one screen (the README's
// "Toward one screen").
#pragma once

#include <CosmicHex/Handheld/PlatformProfile.hpp>

#include <cstddef>

namespace CosmicHex::Handheld::App {

// The stage, in pixels (the top screen)
constexpr int kStageWidth = kThisPlatform.topWidth, kStageHeight = kThisPlatform.topHeight;
// The touch panel, in pixels (the bottom screen)
constexpr int kTouchWidth = kThisPlatform.bottomWidth, kTouchHeight = kThisPlatform.bottomHeight;

// Each one's width against the DS's 256 pixels: what's laid out in DS
// pixels is scaled by it
constexpr float kStageScale = static_cast<float>(kStageWidth) / 256.0f;
constexpr float kTouchScale = static_cast<float>(kTouchWidth) / 256.0f;

// The overlay's text on the stage: letters to a line, and a line's height
constexpr std::size_t kCharsPerLine = kThisPlatform.charsPerLine;
constexpr float kLineHeight = kThisPlatform.lineHeight;

} // namespace CosmicHex::Handheld::App
