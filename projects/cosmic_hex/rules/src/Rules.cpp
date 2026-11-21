// Rules.cpp - Game, the rules as the action log and the bots see them: whose
// turn it is, what an action does (one function a phase, in the file for
// its part of the turn: see RulesInternal.hpp), the legal actions, the
// dealer's draws, and a bot's guess at the hidden cards. See Rules.hpp and
// ../../design.md.
#include "RulesInternal.hpp"

#include <cassert>

namespace CosmicHex {

using namespace Internal;

namespace {

// The turn passes to the next player, who starts with their home's income;
// or, the final round played out, the game ends
void endTurn(State &state) {
  state.player[static_cast<std::size_t>(state.turn)].boughtThisTurn.clear();
  state.cardPlayed = false;
  state.tradingDone = false;
  state.offersMade.clear();
  state.turn = (state.turn + 1) % state.players;
  if (state.turn == 0)
    ++state.round;
  // (finishRound) the final round played out: the game's over
  if (state.finalRound && state.turn == state.first) {
    declareWinners(state);
    return;
  }
  for (Ship &ship : state.ships)
    ship.moved = false;
  state.homeIncomeLeft = homeIncomeFor(state, state.turn);
  nextHomeIncome(state);
}

// The main part of a turn: an action of any part of the game
bool applyMain(State &state, const Action &action) {
  switch (action.kind) {
  case Kind::Trade:
    return applyBankTrade(state, action);
  case Kind::BuildShip:
  case Kind::PlayShipyard:
  case Kind::Move:
  case Kind::Settle:
  case Kind::BuildColony:
    return applyBuildOrMove(state, action);
  case Kind::BuyContract:
    return applyBuyContract(state, action);
  case Kind::RevealContract:
    return applyRevealContract(state, action);
  case Kind::PlayEnforcer:
    return applyEnforcer(state, action);
  case Kind::PlaySupplyDrop:
    return applySupplyDrop(state, action);
  case Kind::PlayCartel:
    return applyCartel(state, action);
  case Kind::CashBadges:
    return applyCashBadges(state, action);
  case Kind::EndTurn:
    endTurn(state);
    return true;
  default:
    return false;
  }
}

// The action, as the part of the turn the game is in takes it; false if
// the rules don't allow it there
bool applyInPhase(State &state, const Action &action) {
  switch (state.phase) {
  case Phase::StartCardsDraw:
    return applyStartCard(state, action);
  case Phase::PlaceColony:
    return applyPlaceColony(state, action);
  case Phase::PlaceShip:
    return applyPlaceShip(state, action);
  case Phase::HomeDieDraw:
  case Phase::HomeIncome:
    return applyHomeIncome(state, action);
  case Phase::HomeNumberDraw:
    return applyHomeNumber(state, action);
  case Phase::RollDraw:
    return applyRoll(state, action);
  case Phase::Blockade:
    return applyBlockade(state, action);
  case Phase::StealDraw:
    return applySteal(state, action);
  case Phase::Trading:
    return applyTrading(state, action);
  case Phase::TradeReply:
    return applyTradeReply(state, action);
  case Phase::TradeCounter:
    return applyTradeCounter(state, action);
  case Phase::Main:
    return applyMain(state, action);
  case Phase::PlanetDraw:
    return applyPlanetDraw(state, action);
  case Phase::ZoneDraw:
    return applyZoneDraw(state, action);
  case Phase::Salvage:
    return applySalvage(state, action);
  case Phase::ContractDraw:
    return applyContractDraw(state, action);
  case Phase::KeepContract:
    return applyKeepContract(state, action);
  case Phase::Over:
    return false;
  }
  return false;
}

// What follows any action: badges cashed (if that's the rule), more tiles
// turned over where a ship stopped, the setup going on once a starting
// ship's finds are turned over, and a win
void afterAnAction(State &state) {
  cashBadgesAtOnce(state);
  if (state.phase == Phase::Main && !state.turning.empty())
    turnNextOver(state);
  finishPlacingShip(state);
  checkForWin(state);
}

// One action for each resource (choosing the home's, taking salvage)
void oneForEachResource(Kind kind, std::vector<Action> &out) {
  for (int r = 0; r < kResources; ++r)
    out.push_back({kind, r});
}

// The main part of a turn: ending it, trading with the bank, the ships and
// stations, the contracts and cards, and cashing badges
void mainActions(const State &state, std::vector<Action> &out) {
  out.push_back({Kind::EndTurn});
  bankTradeActions(state, out);
  buildAndMoveActions(state, out);
  contractActions(state, out);
  cardActions(state, out);
  cashBadgesActions(state, out);
}

} // namespace

int Game::currentPlayer(const State &state) {
  switch (state.phase) {
  case Phase::RollDraw:
  case Phase::StealDraw:
  case Phase::PlanetDraw:
  case Phase::ZoneDraw:
  case Phase::ContractDraw:
  case Phase::HomeNumberDraw:
  case Phase::HomeDieDraw:
  case Phase::StartCardsDraw:
    return GameBots::kDealer;
  case Phase::TradeReply:
    return state.offer.replier;
  default:
    return state.turn;
  }
}

bool Game::apply(State &state, const Action &action) {
  if (state.winner >= 0 || !applyInPhase(state, action))
    return false;
  afterAnAction(state);
  return true;
}

std::vector<Action> Game::legalActions(const State &state) {
  std::vector<Action> out;
  if (state.winner >= 0)
    return out;
  switch (state.phase) {
  case Phase::PlaceColony:
    colonyPlacements(state, out);
    break;
  case Phase::PlaceShip:
    shipPlacements(state, out);
    break;
  case Phase::HomeIncome:
    oneForEachResource(Kind::ChooseHome, out);
    break;
  case Phase::Blockade:
    blockadeActions(state, out);
    break;
  case Phase::Trading:
  case Phase::TradeReply:
  case Phase::TradeCounter:
    tradingActions(state, out);
    break;
  case Phase::Main:
    mainActions(state, out);
    break;
  case Phase::Salvage:
    oneForEachResource(Kind::TakeSalvage, out);
    break;
  case Phase::KeepContract:
    keepContractActions(state, out);
    break;
  default: // the dealer's turn, or over
    break;
  }
  return out;
}

Action Game::dealerAction(const State &state, SeededRandom::Generator &random) {
  // Draws one of several things, each as likely as its count
  const auto weighted = [&](const auto &counts) {
    int sum = 0;
    for (int c : counts)
      sum += c;
    assert(sum > 0);
    int pick = static_cast<int>(random.below(static_cast<std::uint32_t>(sum)));
    for (std::size_t i = 0; i < counts.size(); ++i) {
      if (pick < counts[i])
        return static_cast<int>(i);
      pick -= counts[i];
    }
    return static_cast<int>(counts.size()) - 1;
  };
  switch (state.phase) {
  case Phase::RollDraw:
    return {Kind::Roll, random.between(1, 6), random.between(1, 6)};
  case Phase::StealDraw:
    return {Kind::Steal,
            weighted(state.player[static_cast<std::size_t>(state.victims.front())].resources)};
  case Phase::PlanetDraw: {
    // From the pile: the five kinds, then the moons
    std::array<int, kResources + 1> pile{};
    std::copy(state.kindsLeft.begin(), state.kindsLeft.end(), pile.begin());
    pile[kResources] = state.moonsLeft;
    const int drawn = weighted(pile);
    if (drawn == kResources)
      return {Kind::RevealPlanet, static_cast<int>(PlanetKind::Moon), 0};
    // A number from the pile (by the red-number rule, not a 6 or 8 by a
    // face-up 6 or 8 while there's another)
    std::vector<int> allowed = state.numbersLeft;
    if (state.settings.redNumbersApart && redNumberNextTo(state, state.pendingPlanet) &&
        pileHasOtherThanRed(state))
      allowed.erase(std::remove_if(allowed.begin(), allowed.end(), isRed), allowed.end());
    const int number = allowed[random.below(static_cast<std::uint32_t>(allowed.size()))];
    return {Kind::RevealPlanet, drawn, number};
  }
  case Phase::ZoneDraw:
    return {Kind::RevealFind, weighted(state.findsLeft)};
  case Phase::HomeDieDraw:
    return {Kind::RollHomeDie, static_cast<int>(random.below(kResources + 1))};
  case Phase::StartCardsDraw:
    return {Kind::DealStartCard, static_cast<int>(random.below(kResources))};
  case Phase::HomeNumberDraw: {
    const std::vector<int> &numbers = state.settings.exhaustedNumbers;
    const int number = numbers[random.below(static_cast<std::uint32_t>(numbers.size()))];
    const int kind = state.settings.exhaustedKindRandom
                         ? static_cast<int>(random.below(static_cast<std::uint32_t>(kResources)))
                         : -1;
    return {Kind::NumberHome, number, kind};
  }
  case Phase::ContractDraw: {
    std::vector<int> deck = state.deck;
    random.shuffle(deck.begin(), deck.end());
    return {Kind::DealContracts, deck[0], deck.size() > 1 ? deck[1] : -1};
  }
  default:
    assert(false && "not the dealer's turn");
    return {};
  }
}

State Game::determinize(const State &state, int viewer, SeededRandom::Generator &random) {
  // The only hidden things: the contracts in the other players' hands, and
  // so which are left in the deck. Shuffle the ones the viewer can't place
  // and deal the others' hands again from them.
  State guess = state;
  std::vector<int> unseen = guess.deck;
  for (int p = 0; p < guess.players; ++p) {
    if (p != viewer) {
      const auto &held = guess.player[static_cast<std::size_t>(p)].contracts;
      unseen.insert(unseen.end(), held.begin(), held.end());
    }
  }
  random.shuffle(unseen.begin(), unseen.end());
  std::size_t next = 0;
  for (int p = 0; p < guess.players; ++p) {
    if (p == viewer)
      continue;
    for (int &id : guess.player[static_cast<std::size_t>(p)].contracts)
      id = unseen[next++];
  }
  guess.deck.assign(unseen.begin() + static_cast<std::ptrdiff_t>(next), unseen.end());
  return guess;
}

namespace {
// The rules' tables made when the program starts, not the first time a
// rule asks: a bot's search runs each simulated game in a frame arena
// (ScratchPadd/FrameArena.hpp), and a table first made in one would point at
// memory reused by the next game
const bool kTablesMade = (static_cast<void>(boardCorners()), static_cast<void>(contracts()),
                          static_cast<void>(cornerReachTable()), true);
} // namespace

} // namespace CosmicHex
