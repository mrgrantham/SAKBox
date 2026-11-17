// Describe.hpp - Cosmic Hex's moves in words, for people: "Player 2 builds a
// station." Shared by the balance lab's replays and the console games' move
// lists and histories (the rules' own action text is for log files).
#pragma once

#include <CosmicHex/Rules.hpp>

#include <array>
#include <string>
#include <string_view>

namespace CosmicHex {

// Names to show (the rules' own are for log files)
inline constexpr std::array<std::string_view, kResources> kResourceShown = {
    "minerals", "food", "fuel", "water", "crystals"};
inline constexpr std::array<std::string_view, 7> kKindShown = {
    "asteroid belt", "lush planet", "gas giant", "ice moon", "nebula", "home", "moon"};
inline constexpr std::array<std::string_view, kFinds> kFindShown = {
    "nothing", "salvage", "ruins", "an alien station", "a pirate hideout"};

// Who takes an action in `before`: "Player 2", or "The dealer"
std::string whoActs(const State &before);

// What an action did (or would do), in a sentence: "Player 2 builds a station."
std::string describe(const State &before, const Action &action);

} // namespace CosmicHex
