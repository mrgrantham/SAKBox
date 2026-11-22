// TablePadd.hpp - the game at the table, on its own thread: the dealer's
// draws and the bots' moves (TableGame), one at a time, paced so they can
// be watched, each new state sent to the screens (TableSnapshot)
#pragma once

#include "TableMessages.hpp"

#include <CosmicHex/Handheld/TableGame.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace CosmicHex::Handheld::App {

class TablePadd : public ScratchPadd::Base {
public:
  explicit TablePadd(ScratchPadd::System *system);
  std::string name() override { return "TablePadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::optional<std::chrono::duration<double>> repeatInterval() override;
  // The dealer's draws straight through; then one player's move
  void repeat() override;

private:
  // A new game each time: the seed from the clock
  static std::uint64_t startingSeed();
  void start(const StartGame &start);
  // The table as it is, to the screen
  void sendSnapshot(const TableGame &game);

  std::optional<TableGame> game_;
  std::size_t playedSent_ = 0;                     // (moves already sent in a snapshot)
  std::chrono::steady_clock::time_point sentAt_{}; // (the last snapshot's sending)
  std::uint64_t seed_ = 0;
  bool paused_ = false;
};

} // namespace CosmicHex::Handheld::App
