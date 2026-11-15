// Small games for the bots' tests, each testing one thing:
//   TicTacToe     nothing hidden, no chance: the search must find wins and
//                 blocks
//   DiceRace      chance (the dealer's rolls): the search must beat random
//                 play
//   HiddenNumber  hidden information: the bot must not peek
#pragma once

#include <GameBots/BotGame.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace TestGames {

// Noughts and crosses: player 0 is X and goes first. An action is a square
// 0-8, read left to right, top to bottom.
struct TicTacToe {
  static constexpr std::string_view kName = "tic_tac_toe";
  static constexpr int kRulesVersion = 1;

  struct State {
    std::array<char, 9> squares{'.', '.', '.', '.', '.', '.', '.', '.', '.'};
    int turn = 0;
    bool operator==(const State &) const = default;
  };
  using Action = int;

  static int winner(const State &state) {
    static constexpr int kLines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                                         {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
    for (const auto &line : kLines) {
      const char a = state.squares[line[0]];
      if (a != '.' && a == state.squares[line[1]] && a == state.squares[line[2]])
        return a == 'X' ? 0 : 1;
    }
    return -1;
  }
  static bool full(const State &state) {
    for (char c : state.squares) {
      if (c == '.')
        return false;
    }
    return true;
  }

  static bool apply(State &state, const Action &square) {
    if (square < 0 || square > 8 || state.squares[square] != '.' || isOver(state))
      return false;
    state.squares[square] = state.turn == 0 ? 'X' : 'O';
    state.turn = 1 - state.turn;
    return true;
  }
  static std::string writeState(const State &state) {
    return std::string(state.squares.begin(), state.squares.end()) + " " +
           std::to_string(state.turn) + "\n";
  }
  static std::optional<State> readState(std::string_view text) {
    if (text.size() < 11 || text[9] != ' ')
      return std::nullopt;
    State state;
    for (int i = 0; i < 9; ++i)
      state.squares[i] = text[i];
    state.turn = text[10] - '0';
    return state;
  }
  static std::string writeAction(const Action &square) { return std::to_string(square); }
  static std::optional<Action> readAction(std::string_view text) {
    if (text.size() == 1 && text[0] >= '0' && text[0] <= '8')
      return text[0] - '0';
    return std::nullopt;
  }

  static int playerCount(const State &) { return 2; }
  static int currentPlayer(const State &state) { return state.turn; }
  static bool isOver(const State &state) { return winner(state) >= 0 || full(state); }
  static double score(const State &state, int player) {
    const int w = winner(state);
    return w < 0 ? 0.5 : (w == player ? 1.0 : 0.0);
  }
  static std::vector<Action> legalActions(const State &state) {
    std::vector<Action> out;
    for (int i = 0; i < 9; ++i) {
      if (state.squares[i] == '.')
        out.push_back(i);
    }
    return out;
  }
  static Action dealerAction(const State &, SeededRandom::Generator &) { return 0; } // no chance
  static State determinize(const State &state, int, SeededRandom::Generator &) { return state; }
};
static_assert(GameBots::BotGame<TicTacToe>);

// Two players take turns building a pot: on their turn a player chooses
// Roll (then the dealer rolls a die: a 1 loses the pot and passes the turn,
// anything else goes into the pot) or Hold (bank the pot and pass the turn).
// First to 20 wins.
struct DiceRace {
  static constexpr std::string_view kName = "dice_race";
  static constexpr int kRulesVersion = 1;
  static constexpr int kTarget = 20;

  struct State {
    std::array<int, 2> scores{};
    int pot = 0;
    int turn = 0;
    bool rolling = false; // the player chose to roll: the dealer's turn
    bool operator==(const State &) const = default;
  };
  struct Roll {
    bool operator==(const Roll &) const = default;
  };
  struct Hold {
    bool operator==(const Hold &) const = default;
  };
  struct Face { // the dealer's roll
    int face = 1;
    bool operator==(const Face &) const = default;
  };
  using Action = std::variant<Roll, Hold, Face>;

  static int winner(const State &state) {
    for (int p = 0; p < 2; ++p) {
      if (state.scores[static_cast<std::size_t>(p)] >= kTarget)
        return p;
    }
    return -1;
  }

  static bool apply(State &state, const Action &action) {
    if (isOver(state))
      return false;
    if (const Face *face = std::get_if<Face>(&action)) {
      if (!state.rolling || face->face < 1 || face->face > 6)
        return false;
      state.rolling = false;
      if (face->face == 1) {
        state.pot = 0;
        state.turn = 1 - state.turn;
      } else {
        state.pot += face->face;
      }
      return true;
    }
    if (state.rolling)
      return false;
    if (std::holds_alternative<Roll>(action)) {
      state.rolling = true;
      return true;
    }
    if (state.pot == 0)
      return false;
    state.scores[static_cast<std::size_t>(state.turn)] += state.pot;
    state.pot = 0;
    state.turn = 1 - state.turn;
    return true;
  }
  static std::string writeState(const State &state) {
    return std::to_string(state.scores[0]) + " " + std::to_string(state.scores[1]) + " " +
           std::to_string(state.pot) + " " + std::to_string(state.turn) + " " +
           (state.rolling ? "1" : "0") + "\n";
  }
  static std::optional<State> readState(std::string_view) { return State{}; } // not needed here
  static std::string writeAction(const Action &action) {
    if (const Face *face = std::get_if<Face>(&action))
      return "face " + std::to_string(face->face);
    return std::holds_alternative<Roll>(action) ? "roll" : "hold";
  }
  static std::optional<Action> readAction(std::string_view text) {
    if (text == "roll")
      return Roll{};
    if (text == "hold")
      return Hold{};
    if (text.size() == 6 && text.starts_with("face ") && text[5] >= '1' && text[5] <= '6')
      return Face{text[5] - '0'};
    return std::nullopt;
  }

  static int playerCount(const State &) { return 2; }
  static int currentPlayer(const State &state) {
    return state.rolling ? GameBots::kDealer : state.turn;
  }
  static bool isOver(const State &state) { return winner(state) >= 0; }
  static double score(const State &state, int player) {
    return winner(state) == player ? 1.0 : 0.0;
  }
  static std::vector<Action> legalActions(const State &state) {
    if (state.pot == 0)
      return {Roll{}};
    return {Roll{}, Hold{}};
  }
  static Action dealerAction(const State &, SeededRandom::Generator &random) {
    return Face{random.between(1, 6)};
  }
  static State determinize(const State &state, int, SeededRandom::Generator &) { return state; }
};
static_assert(GameBots::BotGame<DiceRace>);

// The dealer secretly picks a number from 1 to 3 for player 1; player 0
// guesses it, and wins if right. Player 0 can't see the number, so a fair
// bot wins a third of the time, and its guess can't depend on the number.
struct HiddenNumber {
  static constexpr std::string_view kName = "hidden_number";
  static constexpr int kRulesVersion = 1;

  struct State {
    int secret = 0; // 0: not dealt yet
    int guess = 0;  // 0: not guessed yet
    bool operator==(const State &) const = default;
  };
  using Action = int; // the dealer's secret, or player 0's guess

  static bool apply(State &state, const Action &number) {
    if (number < 1 || number > 3 || isOver(state))
      return false;
    (state.secret == 0 ? state.secret : state.guess) = number;
    return true;
  }
  static std::string writeState(const State &state) {
    return std::to_string(state.secret) + " " + std::to_string(state.guess) + "\n";
  }
  static std::optional<State> readState(std::string_view) { return State{}; }
  static std::string writeAction(const Action &number) { return std::to_string(number); }
  static std::optional<Action> readAction(std::string_view text) {
    if (text.size() == 1 && text[0] >= '1' && text[0] <= '3')
      return text[0] - '0';
    return std::nullopt;
  }

  static int playerCount(const State &) { return 2; }
  static int currentPlayer(const State &state) { return state.secret == 0 ? GameBots::kDealer : 0; }
  static bool isOver(const State &state) { return state.guess != 0; }
  static double score(const State &state, int player) {
    const bool right = state.guess == state.secret;
    return (player == 0) == right ? 1.0 : 0.0;
  }
  static std::vector<Action> legalActions(const State &) { return {1, 2, 3}; }
  static Action dealerAction(const State &, SeededRandom::Generator &random) {
    return random.between(1, 3);
  }
  // Player 0 can't see the secret: a guess at it, from what's possible
  static State determinize(const State &state, int viewer, SeededRandom::Generator &random) {
    State guess = state;
    if (viewer == 0 && guess.secret != 0)
      guess.secret = random.between(1, 3);
    return guess;
  }
};
static_assert(GameBots::BotGame<HiddenNumber>);

} // namespace TestGames
