// ReplayView.hpp - watching a game in the balance lab: the board as it was
// at any point (planets turned over with their kinds and numbers, the zone's
// finds, stations, colonies, ships, the blockade), what just happened, each
// player's points, resources and badges, and the points through the game as
// a chart (click it to jump there). Step an action or a turn at a time,
// forwards or back, or play it at a chosen speed.
#pragma once

#include "GameReplay.hpp"

#include <string>

namespace CosmicHex::Lab {

class ReplayView {
public:
  // Shows a game from its start; `title` says which ("Game 12 of R2...")
  void show(GameReplay replay, std::string title);
  bool empty() const { return replay_.states.empty(); }
  void goTo(std::size_t step);
  // Draws it in the current ImGui window, `width` pixels wide
  void draw(float width);

private:
  void drawControls();
  void drawPlayers(const State &state);
  void drawPointsChart();
  void drawRecentActions();

  GameReplay replay_;
  std::string title_;
  std::size_t step_ = 0;        // the state shown: after this many actions
  bool playing_ = false;        // stepping on by itself
  float actionsASecond_ = 8.0f; // when playing
  float sinceStep_ = 0.0f;      // seconds since the last step while playing
};

} // namespace CosmicHex::Lab
