// Palette.hpp - Cosmic Hex's colours on the consoles, each named for its
// role, so the screens use them the same way everywhere:
//
//   kAccent      turquoise: what's always there, and whose turn it is (the
//                round's hexagon, the turn's points bar, the turn banner's
//                edges, the interface's outlines)
//   kHighlight   amber: moments and choices (the roll, what's selected or
//                chosen, a hint)
//   kWords, kSoftWords, kDimWords   text, from most to least prominent
//   kSpace       the board's background
//   player(p)    each player's colour: red, blue, white, yellow
//   inkOnBand(band)  a card title's letters on its band: dark on a light
//                band, light on a dark one
//
//   overlay.text(x, y, "ROUND", Palette::kDimWords);
//   overlay.rectangle(x, y, w, 1, Palette::withAlpha(Palette::kAccent, 128));
#pragma once

#include <CosmicHex/Rules.hpp>

#include <ScratchPadd/Scene/Types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace CosmicHex::Handheld::Palette {

using Colour = ScratchPadd::Scene::Colour;

inline constexpr Colour kAccent{64, 224, 208};
inline constexpr Colour kHighlight{255, 214, 90};
inline constexpr Colour kWords{226, 232, 244};
inline constexpr Colour kSoftWords{200, 208, 222};
inline constexpr Colour kDimWords{138, 148, 168};
inline constexpr Colour kSpace{24, 34, 55};

inline constexpr std::array<Colour, kMaxPlayers> kPlayers{
    Colour{235, 70, 70}, Colour{70, 140, 245}, Colour{245, 245, 245}, Colour{250, 200, 40}};
constexpr Colour player(int p) { return kPlayers[static_cast<std::size_t>(p) % kPlayers.size()]; }

// A card's title letters on its label band (every card view, both
// screens): dark on a light band, light on a dark one, by the band's
// luminance (Rec. 709). The user's: white titles on the pale bands were
// hard to read; Show resources' piles already wrote them dark
inline constexpr Colour kInkOnLightBand{16, 20, 30};
inline constexpr Colour kInkOnDarkBand{242, 245, 251};
constexpr Colour inkOnBand(Colour band) {
  constexpr float kLightest = 255;
  constexpr float kLightBand = 0.55f * kLightest; // (above it, a band reads as light)
  const float luminance = 0.2126f * static_cast<float>(band.r) +
                          0.7152f * static_cast<float>(band.g) +
                          0.0722f * static_cast<float>(band.b);
  return luminance > kLightBand ? kInkOnLightBand : kInkOnDarkBand;
}

// The same colour, `alpha` opaque (0 to 255)
constexpr Colour withAlpha(Colour colour, std::uint8_t alpha) {
  colour.a = alpha;
  return colour;
}

} // namespace CosmicHex::Handheld::Palette
