// ScriptedRuns.hpp - what the scripted-play tests share: a game played
// through the app's own shows and touch panel (ScriptedGame), the short
// games' settings, and the shows cut short or skipped without the player
// asking, in words (the README's "Scripted play")
#pragma once

#include <CosmicHex/Handheld/ScriptedGame.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Handheld::Testing {

// A whole game, played (made where it stays: its hooks hold on to it)
std::unique_ptr<ScriptedGame> played(const ScriptedGameSettings &settings);

// A short game under the agreed rules but for two of the game's own
// settings: a home exhausted at kShortExhaustAt points, the win at
// kShortWinPoints (seeds 1 to 32: 1,110 s of play on average, against
// 2,319 s for full-length games)
inline constexpr int kShortWinPoints = 8;
inline constexpr int kShortExhaustAt = 4;
ScriptedGameSettings shortGame(ScriptedGameSettings settings);

// Short games, one for each seed in [first, first + count), with the rest
// of `settings`
std::vector<ScriptedGameSettings> shortGames(std::uint64_t first, int count,
                                             const ScriptedGameSettings &settings = {});

// A game played and what it was played with
struct PlayedGame {
  ScriptedGameSettings settings;
  std::unique_ptr<ScriptedGame> game;
};
std::vector<PlayedGame> playAll(const std::vector<ScriptedGameSettings> &games);
// The run in words, for a failure's message ("seed 7, Quick")
std::string nameOf(const ScriptedGameSettings &settings);

// Each show (of `kind`, or any) cut short or skipped without the player
// asking, a line each: "" when every show was seen whole or skipped by the
// player
std::string unasked(const ScriptedGame &game, std::optional<ShowKind> kind = std::nullopt);

// The game ended as a game does: the table's winner, with the points to
// win, celebrated (Animations Full or Quick); "" if so, else what's wrong
std::string endedByTheWin(const ScriptedGame &game);

} // namespace CosmicHex::Handheld::Testing
