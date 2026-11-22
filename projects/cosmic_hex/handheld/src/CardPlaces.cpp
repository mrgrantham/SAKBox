#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/CardPlaces.hpp>
#include <CosmicHex/Handheld/Hud.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>

namespace CosmicHex::Handheld::CardPlaces {

namespace {
// Sizes are a DS's pixels, scaled to the screen (TopFrame::k)
float scaleOf(float width) { return width / 256; }
} // namespace

Point bank(float width) {
  const float k = scaleOf(width);
  return {kRoundHexagonX * k, kRoundHexagonY * k};
}

Point yourHand(float width, float height) { return {width * 0.7f, height + 24 * scaleOf(width)}; }

Point playerRow(const State &state, float width, int player) {
  const PointsBars::Row row = PointsBars::rowOf(player, width, state.settings.winPoints);
  return {row.middleX(), row.middleY()};
}

Point of(const State &state, float width, float height, int who, int you) {
  if (who == kBank)
    return bank(width);
  return who == you ? yourHand(width, height) : playerRow(state, width, who);
}

std::optional<Hex> homeOf(const State &state, int player) {
  std::optional<Hex> former;
  for (const Planet &planet : state.planets) {
    if (planet.homeOf == player)
      return planet.hex;
    if (planet.formerHomeOf == player)
      former = planet.hex;
  }
  return former;
}

Point at(const TopFrame &frame, const State &state, int who, int you) {
  return of(state, frame.width, frame.height, who, you);
}

Point paidFrom(const TopFrame &frame, const State &state, int payer, int you) {
  if (const std::optional<Hex> home = homeOf(state, payer)) {
    Point onScreen;
    if (frame.renderer.toScreen(BoardPieces::centre(*home), onScreen.x, onScreen.y))
      return onScreen;
  }
  return at(frame, state, payer, you);
}

Point flightStart(const TopFrame &frame, const State &state, const RollPayShow::Card &card,
                  int you) {
  if (card.planet >= 0 && card.planet < static_cast<int>(state.planets.size())) {
    Point onScreen{frame.width / 2, frame.height / 2};
    frame.renderer.toScreen(
        BoardPieces::centre(state.planets[static_cast<std::size_t>(card.planet)].hex), onScreen.x,
        onScreen.y);
    return onScreen;
  }
  if (card.player == kBank && card.from != kBank)
    return paidFrom(frame, state, card.from, you);
  return at(frame, state, card.from, you);
}

} // namespace CosmicHex::Handheld::CardPlaces
