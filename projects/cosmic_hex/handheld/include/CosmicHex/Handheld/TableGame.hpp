// TableGame.hpp - a game of Cosmic Hex as a console plays it: some seats
// people, the rest bots, and the dealer's dice and draws, one move at a
// time, kept in an action log (so it can be saved, replayed, or watched in
// the balance lab), with each move in words for the screen.
//
//   TableGame game({.seats = {Seat::Person, Seat::Bot, Seat::Bot, Seat::Bot}});
//   while (!game.over()) {
//     if (game.waitingForPerson())
//       game.play(chosenMove);   // a legal move (Game::legalActions)
//     else
//       game.step();             // the dealer or a bot moves
//   }
//
// No padds and no screens: the console's app runs it on its own padd (a
// bot can think for a while), and the Mac's tests play it.
#pragma once

#include <CosmicHex/Rules.hpp>

#include <ActionLog/ActionLog.hpp>
#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

enum class Seat : std::uint8_t { Person, Bot };

struct TableSettings {
  std::array<Seat, kMaxPlayers> seats{Seat::Bot, Seat::Bot, Seat::Bot, Seat::Bot};
  int players = 4;
  // How hard the bots think: simulated games a move (the Mac's batches use
  // 400; a console far fewer, to answer in about a second)
  int botIterations = 40;
  // Or by time: a bot thinks up to this many seconds a move (and at most
  // botIterations, which a fast machine reaches first); 0: by iterations
  // alone (the Mac's tests, which must play the same every run)
  double botSeconds = 0;
  std::uint64_t seed = 1;
  Board board = boardR3();
};

class TableGame {
public:
  explicit TableGame(const TableSettings &settings);

  const State &state() const { return play_.state(); }
  const ActionLog::Log<Game> &log() const { return play_.log(); }
  bool over() const { return Game::isOver(state()); }

  // Whether it's a person's move (theirs is the current player)
  bool waitingForPerson() const;

  // The dealer's or a bot's move, made: what it was, or nothing if it's a
  // person's move or the game is over
  std::optional<Action> step();

  // The moves a person can make now: the rules' legal moves, and in their
  // trading step the plain offers to anyone (one of a resource they hold
  // for one of another, and two for one where they can), which the rules'
  // own list leaves out (offers are too many for the bots' search)
  std::vector<Action> personMoves() const;

  // A person's move: false if it isn't theirs to make or isn't allowed
  bool play(const Action &move);

  // Every move so far, in words ("Player 2 builds a station."), oldest first
  const std::vector<std::string> &history() const { return history_; }

private:
  void record(const State &before, const Action &move);

  TableSettings settings_;
  ActionLog::Play<Game> play_;
  SeededRandom::Generator dealer_, bots_;
  GameBots::Ismcts<Game> ismcts_;
  std::vector<std::string> history_;
};

} // namespace CosmicHex::Handheld
