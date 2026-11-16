#include "ReplayView.hpp"

#include "BoardDrawing.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <utility>

namespace CosmicHex::Lab {

namespace {

// The players' colours: bright, so their pieces stand out from the planets
constexpr std::array<ImU32, kMaxPlayers> kPlayerColours = {
    IM_COL32(235, 88, 88, 255), IM_COL32(88, 150, 245, 255), IM_COL32(240, 214, 80, 255),
    IM_COL32(190, 120, 240, 255)};
constexpr std::array<ImVec4, kMaxPlayers> kPlayerColoursF = {
    ImVec4(0.92f, 0.35f, 0.35f, 1), ImVec4(0.35f, 0.59f, 0.96f, 1), ImVec4(0.94f, 0.84f, 0.31f, 1),
    ImVec4(0.75f, 0.47f, 0.94f, 1)};

// A face-up planet's colour, by kind: pastel, so pieces show on them
constexpr std::array<ImU32, 7> kKindColours = {
    IM_COL32(170, 152, 132, 255), // asteroid belt (minerals)
    IM_COL32(140, 214, 150, 255), // lush planet (food)
    IM_COL32(232, 170, 110, 255), // gas giant (fuel)
    IM_COL32(150, 200, 236, 255), // ice moon (water)
    IM_COL32(206, 176, 238, 255), // nebula (crystals)
    IM_COL32(240, 180, 90, 255),  // home
    IM_COL32(120, 124, 136, 255), // moon
};
constexpr ImU32 kFaceDown = IM_COL32(64, 98, 74, 255); // a planet not yet found
constexpr ImU32 kRedNumber = IM_COL32(190, 30, 30, 255);
constexpr ImU32 kOutline = IM_COL32(16, 16, 20, 255);
constexpr ImU32 kChanged = IM_COL32(255, 255, 255, 255); // what the last action changed

ImU32 playerColour(int player) {
  return player >= 0 && player < kMaxPlayers ? kPlayerColours[static_cast<std::size_t>(player)]
                                             : kText;
}

// The resources' short names, for the players' table
constexpr std::array<const char *, kResources> kResourceShort = {"Min", "Food", "Fuel", "Water",
                                                                 "Cry"};

void drawShip(ImDrawList *draw, ImVec2 at, float size, ImU32 colour) {
  const float s = size * 0.3f;
  const ImVec2 a(at.x, at.y - s), b(at.x - s * 0.87f, at.y + s * 0.6f),
      c(at.x + s * 0.87f, at.y + s * 0.6f);
  draw->AddTriangleFilled(a, b, c, colour);
  draw->AddTriangle(a, b, c, kOutline, 1.5f);
}

void drawStation(ImDrawList *draw, ImVec2 at, float size, ImU32 colour, bool colony) {
  const float r = size * (colony ? 0.3f : 0.2f);
  draw->AddCircleFilled(at, r, colour);
  draw->AddCircle(at, r, kOutline, 0, 1.5f);
  if (colony)
    draw->AddCircle(at, r * 0.55f, kOutline, 0, 1.5f); // (a ring inside: a colony)
}

// The board as it is in `state`; marks what changed since `before`
void drawState(ImDrawList *draw, const HexGrid::Layout &at, const State &state,
               const State &before) {
  const float size = at.size.x;
  // Empty space, then the zone's tiles, then the planets
  for (Hex hex : HexGrid::spiral(Hex{}, kBoardRadius))
    drawHex(draw, at, hex, kSpace);
  for (std::size_t i = 0; i < state.zone.size(); ++i) {
    const ZoneSite &site = state.zone[i];
    drawHex(draw, at, site.hex, kFind);
    if (site.flipped) {
      static constexpr std::array<const char *, kFinds> kLetters = {"-", "S", "R", "A", "P"};
      centredText(draw, toImVec(at.centre(site.hex)), kText,
                  kLetters[static_cast<std::size_t>(site.find)]);
    }
  }
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    const Planet &planet = state.planets[i];
    const ImVec2 centre = toImVec(at.centre(planet.hex));
    if (!planet.flipped) {
      drawHex(draw, at, planet.hex, kFaceDown);
      centredText(draw, centre, kText, planet.core ? "*" : "?");
      continue;
    }
    if (planet.homeOf >= 0) {
      drawHex(draw, at, planet.hex, kHome);
      draw->AddCircle(centre, size * 0.62f, playerColour(planet.homeOf), 0, 3.0f);
      centredText(draw, centre, kDark, "H");
      continue;
    }
    drawHex(draw, at, planet.hex, kKindColours[static_cast<std::size_t>(planet.kind)]);
    if (planet.number > 0) {
      const std::string number = std::to_string(planet.number);
      centredText(draw, centre, planet.number == 6 || planet.number == 8 ? kRedNumber : kDark,
                  number.c_str());
    }
    if (planet.core)
      centredText(draw, ImVec2(centre.x, centre.y - size * 0.45f), kDark, "*");
  }
  // What the last action turned over
  for (std::size_t i = 0; i < state.planets.size() && i < before.planets.size(); ++i)
    if (state.planets[i].flipped && !before.planets[i].flipped)
      draw->AddCircle(toImVec(at.centre(state.planets[i].hex)), size * 0.75f, kChanged, 0, 2.5f);
  for (std::size_t i = 0; i < state.zone.size() && i < before.zone.size(); ++i)
    if (state.zone[i].flipped && !before.zone[i].flipped)
      draw->AddCircle(toImVec(at.centre(state.zone[i].hex)), size * 0.75f, kChanged, 0, 2.5f);
  // The blockade: a dark ring and an X on its planet
  if (state.blockade >= 0 && state.blockade < static_cast<int>(state.planets.size())) {
    const ImVec2 c =
        toImVec(at.centre(state.planets[static_cast<std::size_t>(state.blockade)].hex));
    const float s = size * 0.35f;
    draw->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), kOutline, 3.0f);
    draw->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x + s, c.y - s), kOutline, 3.0f);
  }

  // Stations and colonies: on corners, or (without corners) on their planets
  for (std::size_t i = 0; i < state.stations.size(); ++i) {
    const Station &station = state.stations[i];
    const ImVec2 p = toImVec(at.point(station.at));
    drawStation(draw, p, size, playerColour(station.owner), station.colony);
    const bool isNew = i >= before.stations.size() || before.stations[i].colony != station.colony;
    if (isNew)
      draw->AddCircle(p, size * 0.42f, kChanged, 0, 2.5f);
  }
  if (!state.settings.corners) {
    for (const Planet &planet : state.planets)
      for (std::size_t d = 0; d < kDocks; ++d)
        if (planet.docks[d] >= 0) {
          const ImVec2 c = toImVec(at.centre(planet.hex));
          const float dx = (d == 0 ? -1.0f : 1.0f) * size * 0.4f;
          drawStation(draw, ImVec2(c.x + dx, c.y + size * 0.4f), size * 0.8f,
                      playerColour(planet.docks[d]), planet.colony[d]);
        }
  }
  // Ships, and where one just moved to
  for (std::size_t i = 0; i < state.ships.size(); ++i) {
    const Ship &ship = state.ships[i];
    ImVec2 p;
    if (state.settings.corners) {
      p = toImVec(at.point(ship.corner));
    } else {
      const ImVec2 c = toImVec(at.centre(ship.at));
      p = ImVec2(c.x + size * 0.35f * static_cast<float>(i % 3) - size * 0.35f, c.y - size * 0.3f);
    }
    drawShip(draw, p, size, playerColour(ship.owner));
    const bool moved = i >= before.ships.size() || before.ships[i].corner != ship.corner ||
                       before.ships[i].at != ship.at;
    if (moved)
      draw->AddCircle(p, size * 0.42f, kChanged, 0, 2.5f);
  }
}

} // namespace

void ReplayView::show(GameReplay replay, std::string title) {
  replay_ = std::move(replay);
  title_ = std::move(title);
  step_ = 0;
  playing_ = false;
  sinceStep_ = 0;
}

void ReplayView::goTo(std::size_t step) { step_ = std::min(step, replay_.last()); }

void ReplayView::draw(float width) {
  if (empty())
    return;
  // Playing: step on as time passes
  if (playing_) {
    sinceStep_ += ImGui::GetIO().DeltaTime;
    const float every = 1.0f / std::max(actionsASecond_, 0.5f);
    while (sinceStep_ >= every && step_ < replay_.last()) {
      sinceStep_ -= every;
      ++step_;
    }
    if (step_ >= replay_.last())
      playing_ = false;
  }
  const State &state = replay_.at(step_);
  const State &before = replay_.at(step_ == 0 ? 0 : step_ - 1);

  ImGui::TextUnformatted(title_.c_str());
  if (!replay_.problem.empty())
    ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "Stopped early: %s", replay_.problem.c_str());
  drawControls();

  // The board
  // (pointy-topped, the board is 14 hex sizes tall to 15.6 wide: its box
  // as tall as it needs, so the players and the chart fit below)
  const float side = std::min(width, 450.0f);
  const float size = side / kBoardAcross, tall = size * 14.6f;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(side, tall));
  drawState(ImGui::GetWindowDrawList(),
            boardLayout(ImVec2(origin.x + side / 2, origin.y + tall / 2), size), state, before);

  // What just happened
  drawRecentActions();
  ImGui::Separator();
  drawPlayers(state);
  drawPointsChart();
  ImGui::TextDisabled("? a planet not yet found, * the core; numbers: 6 and 8 in red;"
                      " S salvage, R ruins, A alien station, P pirates; X the blockade;"
                      " white rings: what the last action changed");
}

void ReplayView::drawControls() {
  const State &state = replay_.at(step_);
  if (ImGui::Button("|<"))
    goTo(0);
  ImGui::SameLine();
  if (ImGui::Button("< turn"))
    goTo(replay_.previousTurn(step_));
  ImGui::SameLine();
  if (ImGui::Button("<"))
    goTo(step_ == 0 ? 0 : step_ - 1);
  ImGui::SameLine();
  if (ImGui::Button(playing_ ? "Pause" : "Play", ImVec2(60, 0))) {
    playing_ = !playing_;
    sinceStep_ = 0;
    if (playing_ && step_ >= replay_.last())
      step_ = 0; // (from the start again)
  }
  ImGui::SameLine();
  if (ImGui::Button(">"))
    goTo(step_ + 1);
  ImGui::SameLine();
  if (ImGui::Button("turn >"))
    goTo(replay_.nextTurn(step_));
  ImGui::SameLine();
  if (ImGui::Button(">|"))
    goTo(replay_.last());
  ImGui::SameLine();
  ImGui::PushItemWidth(110);
  ImGui::SliderFloat("a second", &actionsASecond_, 1, 60, "%.0f actions");
  ImGui::PopItemWidth();
  int step = static_cast<int>(step_);
  ImGui::PushItemWidth(-1);
  if (ImGui::SliderInt("##step", &step, 0, static_cast<int>(replay_.last()), "action %d"))
    goTo(static_cast<std::size_t>(step));
  ImGui::PopItemWidth();
  const char *setup =
      state.phase == Phase::PlaceColony || state.phase == Phase::PlaceShip ? " (setting up)" : "";
  ImGui::Text("Round %d%s, Player %d's turn; action %zu of %zu", state.round, setup, state.turn + 1,
              step_, replay_.last());
}

void ReplayView::drawRecentActions() {
  // The last few actions, the latest first and brightest
  if (step_ == 0) {
    ImGui::TextUnformatted("The start.");
    for (int i = 0; i < 3; ++i)
      ImGui::TextUnformatted("");
    return;
  }
  for (int back = 0; back < 4; ++back) {
    if (step_ < static_cast<std::size_t>(back) + 1) {
      ImGui::TextUnformatted("");
      continue;
    }
    const std::size_t i = step_ - 1 - static_cast<std::size_t>(back);
    const std::string text =
        std::to_string(i + 1) + ". " + describe(replay_.states[i], replay_.actions[i]);
    if (back == 0)
      ImGui::TextUnformatted(text.c_str());
    else
      ImGui::TextDisabled("%s", text.c_str());
  }
}

void ReplayView::drawPlayers(const State &state) {
  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
  if (!ImGui::BeginTable("players", 6 + kResources, kFlags))
    return;
  for (const char *heading : {"", "Player", "Order", "Points", "Badges", "Stations"})
    ImGui::TableSetupColumn(heading);
  for (const char *resource : kResourceShort)
    ImGui::TableSetupColumn(resource);
  ImGui::TableHeadersRow();
  for (int p = 0; p < state.players; ++p) {
    const auto pi = static_cast<std::size_t>(p);
    const Player &player = state.player[pi];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(p == state.turn && state.winner < 0 ? ">" : "");
    ImGui::TableNextColumn();
    ImGui::TextColored(kPlayerColoursF[pi], "Player %d%s", p + 1, state.winner == p ? " wins" : "");
    ImGui::TableNextColumn();
    static constexpr std::array<const char *, kMaxPlayers> kOrder = {"1st", "2nd", "3rd", "4th"};
    ImGui::TextUnformatted(
        kOrder[static_cast<std::size_t>((p - state.first + state.players) % state.players)]);
    ImGui::TableNextColumn();
    ImGui::Text("%d", points(state, p));
    ImGui::TableNextColumn();
    ImGui::Text("%d", player.badges);
    ImGui::TableNextColumn();
    ImGui::Text("%d", stationsOf(state, p));
    for (int r : player.resources) {
      ImGui::TableNextColumn();
      ImGui::Text("%d", r);
    }
  }
  ImGui::EndTable();
}

void ReplayView::drawPointsChart() {
  // (no legend: the players' table above has their colours)
  if (!ImPlot::BeginPlot("Points through the game", ImVec2(-1, 150),
                         ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText |
                             ImPlotFlags_NoLegend))
    return;
  ImPlot::SetupAxes("action (click to go there)", "points", ImPlotAxisFlags_None,
                    ImPlotAxisFlags_AutoFit);
  ImPlot::SetupAxisLimits(ImAxis_X1, 0, static_cast<double>(replay_.last()), ImPlotCond_Always);
  const int players = replay_.states.front().players;
  for (int p = 0; p < players; ++p) {
    const auto &points = replay_.points[static_cast<std::size_t>(p)];
    const std::string name = "Player " + std::to_string(p + 1);
    ImPlot::PlotLine(name.c_str(), points.data(), static_cast<int>(points.size()), 1.0, 0.0,
                     ImPlotSpec(ImPlotProp_LineColor, kPlayerColoursF[static_cast<std::size_t>(p)],
                                ImPlotProp_LineWeight, 2.0f));
  }
  const double now[] = {static_cast<double>(step_)};
  ImPlot::PlotInfLines("now", now, 1);
  if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    const double x = ImPlot::GetPlotMousePos().x;
    goTo(static_cast<std::size_t>(std::max(0.0, x + 0.5)));
  }
  ImPlot::EndPlot();
}

} // namespace CosmicHex::Lab
