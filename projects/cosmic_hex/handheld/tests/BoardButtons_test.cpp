// The board's buttons from the game (BoardButtons::situationOf): nothing
// but Roll off your move; End Turn, Trade and Cards from your legal moves
// and hand; and End Turn shown dimmed while a new ship must leave its colony
#include <CosmicHex/Handheld/BoardButtons.hpp>

#include "TestTables.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Kind = Action::Kind;

namespace {
// A main part with a ship of the player to move standing on their colony,
// not moved yet, and new ships made to leave (Settings::newShipsLeave)
State aNewShipOnItsColony() {
  State state = TestTables::atFirstMain();
  state.settings.newShipsLeave = true;
  const Corner colony{{0, 0}, 0};
  state.stations = {Station{colony, state.turn, true}};
  state.ships = {Ship{state.turn, Hex{}, false, colony}};
  return state;
}
} // namespace

// Off your move nothing is yours to use, cards held or not; only your roll
// waiting shows Roll
TEST(BoardButtonsTest, OffYourMoveOnlyYourRoll) {
  State state = TestTables::atFirstMain();
  state.player[static_cast<std::size_t>(state.turn)].contracts = {kEnforcerCard};
  const std::vector<Action> legal = Game::legalActions(state);
  const BoardButtons::Situation waiting = BoardButtons::situationOf(state, legal, false);
  EXPECT_FALSE(waiting.yourMove);
  EXPECT_FALSE(waiting.canRoll);
  EXPECT_FALSE(waiting.canEndTurn);
  EXPECT_FALSE(waiting.canTrade);
  EXPECT_FALSE(waiting.holdsCards);
  EXPECT_TRUE(BoardButtons::situationOf(state, legal, false, true).canRoll);
}

// On your move: End Turn from the rules' EndTurn or EndTrading, Trade from a
// bank trade, an offer or the turn's offers to end, Cards from a hand
TEST(BoardButtonsTest, YourMovesButtonsFromTheLegalMoves) {
  State state = TestTables::atFirstMain();
  const auto situation = [&](const std::vector<Action> &legal) {
    return BoardButtons::situationOf(state, legal, true);
  };
  EXPECT_FALSE(situation({}).canEndTurn);
  EXPECT_FALSE(situation({}).canTrade);
  EXPECT_TRUE(situation({{Kind::EndTurn}}).canEndTurn);
  EXPECT_FALSE(situation({{Kind::EndTurn}}).canTrade);
  EXPECT_TRUE(situation({{Kind::Trade}}).canTrade);
  EXPECT_TRUE(situation({{Kind::OfferTrade}}).canTrade);
  const BoardButtons::Situation trading = situation({{Kind::EndTrading}});
  EXPECT_TRUE(trading.canTrade);
  EXPECT_TRUE(trading.canEndTurn);
  EXPECT_FALSE(situation({}).holdsCards);
  state.player[static_cast<std::size_t>(state.turn)].contracts = {kEnforcerCard};
  EXPECT_TRUE(situation({}).holdsCards);
}

// A new ship still on its colony, with new ships made to leave: End Turn
// shows (dimmed) to say so; not once it has moved, nor when the turn could
// end anyway, nor under the old rule
TEST(BoardButtonsTest, ANewShipMustLeaveItsColonyFirst) {
  State state = aNewShipOnItsColony();
  EXPECT_TRUE(BoardButtons::situationOf(state, {}, true).newShipMustMove);
  EXPECT_FALSE(BoardButtons::situationOf(state, {{Kind::EndTurn}}, true).newShipMustMove);
  EXPECT_FALSE(BoardButtons::situationOf(state, {}, false).newShipMustMove);
  state.ships[0].moved = true;
  EXPECT_FALSE(BoardButtons::situationOf(state, {}, true).newShipMustMove);
  state = aNewShipOnItsColony();
  state.settings.newShipsLeave = false;
  EXPECT_FALSE(BoardButtons::situationOf(state, {}, true).newShipMustMove);
  state = aNewShipOnItsColony();
  state.ships[0].owner = (state.turn + 1) % state.players; // (someone else's)
  EXPECT_FALSE(BoardButtons::situationOf(state, {}, true).newShipMustMove);
}
