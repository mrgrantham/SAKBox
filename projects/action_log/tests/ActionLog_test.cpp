// The action log, with a small dice game: two players take turns rolling a
// die into a pot, and either hold (banking the pot) or keep rolling; a 1
// loses the pot and ends the turn. First to 20 wins. The dealer's rolls
// are actions like the players' holds.
#include <ActionLog/ActionLog.hpp>

#include <SeededRandom/Seed.hpp>

#include <gtest/gtest.h>

#include <array>
#include <sstream>
#include <variant>

namespace {

struct DiceRace {
  static constexpr std::string_view kName = "dice_race";
  static constexpr int kRulesVersion = 1;
  static constexpr int kTarget = 20;

  struct State {
    std::array<int, 2> scores{};
    int pot = 0;
    int turn = 0;
    int winner = -1;
    bool operator==(const State &) const = default;
  };
  struct Roll { // the dealer's
    int face = 1;
    bool operator==(const Roll &) const = default;
  };
  struct Hold {
    bool operator==(const Hold &) const = default;
  };
  using Action = std::variant<Roll, Hold>;

  static bool apply(State &state, const Action &action) {
    if (state.winner >= 0)
      return false;
    if (const Roll *roll = std::get_if<Roll>(&action)) {
      if (roll->face < 1 || roll->face > 6)
        return false;
      if (roll->face == 1) {
        state.pot = 0;
        state.turn = 1 - state.turn;
      } else {
        state.pot += roll->face;
      }
      return true;
    }
    if (state.pot == 0) // nothing to hold
      return false;
    state.scores[static_cast<std::size_t>(state.turn)] += state.pot;
    state.pot = 0;
    if (state.scores[static_cast<std::size_t>(state.turn)] >= kTarget)
      state.winner = state.turn;
    state.turn = 1 - state.turn;
    return true;
  }

  static std::string writeState(const State &state) {
    std::ostringstream out;
    out << "scores " << state.scores[0] << " " << state.scores[1] << "\n"
        << "pot " << state.pot << "\nturn " << state.turn << "\nwinner " << state.winner << "\n";
    return out.str();
  }
  static std::optional<State> readState(std::string_view text) {
    std::istringstream in{std::string(text)};
    State state;
    std::string scores, pot, turn, winner;
    if (!(in >> scores >> state.scores[0] >> state.scores[1] >> pot >> state.pot >> turn >>
          state.turn >> winner >> state.winner) ||
        scores != "scores" || pot != "pot" || turn != "turn" || winner != "winner")
      return std::nullopt;
    return state;
  }
  static std::string writeAction(const Action &action) {
    if (const Roll *roll = std::get_if<Roll>(&action))
      return "roll " + std::to_string(roll->face);
    return "hold";
  }
  static std::optional<Action> readAction(std::string_view text) {
    if (text == "hold")
      return Hold{};
    if (text.size() == 6 && text.starts_with("roll ") && text[5] >= '1' && text[5] <= '6')
      return Roll{text[5] - '0'};
    return std::nullopt;
  }
};
static_assert(ActionLog::Game<DiceRace>);

using Play = ActionLog::Play<DiceRace>;

// A whole game: the dealer rolls from the seed's "dice" stream, and each
// player holds once the pot reaches 8.
Play playAGame(std::uint64_t seed) {
  Play play(DiceRace::State{}, 5);
  play.addMeta("players", "Ann, Bot");
  SeededRandom::Generator dice = SeededRandom::Seed(seed).stream("dice");
  while (play.state().winner < 0) {
    if (play.state().pot >= 8)
      EXPECT_TRUE(play.play(DiceRace::Hold{}));
    else
      EXPECT_TRUE(play.play(DiceRace::Roll{dice.between(1, 6)}));
  }
  return play;
}

} // namespace

TEST(ActionLogTest, OnlyAllowedActionsAreRecorded) {
  Play play(DiceRace::State{});
  EXPECT_FALSE(play.play(DiceRace::Hold{})); // nothing in the pot
  EXPECT_TRUE(play.play(DiceRace::Roll{4}));
  EXPECT_FALSE(play.play(DiceRace::Roll{7}));
  EXPECT_TRUE(play.play(DiceRace::Hold{}));
  EXPECT_EQ(play.log().actions.size(), 2u);
  EXPECT_EQ(play.state().scores[0], 4);
}

// A recorded game replays to the same end, from the log alone: no seed.
TEST(ActionLogTest, AGameReplaysExactly) {
  const Play play = playAGame(7);
  const ActionLog::Replay<DiceRace> again = ActionLog::replay(play.log());
  ASSERT_TRUE(again.ok()) << again.problem.value_or(ActionLog::Problem{}).why;
  EXPECT_EQ(again.state, play.state());
  EXPECT_EQ(again.played, play.log().actions.size());
  EXPECT_GE(again.state.winner, 0);
}

// Written as text and read back, the log is the same, and replays the same.
TEST(ActionLogTest, TextReadsBackAsTheSameLog) {
  const Play play = playAGame(11);
  const std::string text = ActionLog::write(play.log());
  const auto log = ActionLog::read<DiceRace>(text);
  ASSERT_TRUE(log.has_value()) << log.error().line << ": " << log.error().why;
  EXPECT_EQ(log->start, play.log().start);
  EXPECT_EQ(log->actions, play.log().actions);
  EXPECT_EQ(log->fingerprints, play.log().fingerprints);
  EXPECT_EQ(log->meta, play.log().meta);
  EXPECT_EQ(ActionLog::write(*log), text);
  EXPECT_EQ(ActionLog::replay(*log).state, play.state());
}

// The text: a header, the start, one action a line, and fingerprints.
TEST(ActionLogTest, TheTextLooksLikeThis) {
  Play play(DiceRace::State{}, 2);
  play.play(DiceRace::Roll{4});
  play.play(DiceRace::Roll{3});
  play.play(DiceRace::Hold{});
  const std::string text = ActionLog::write(play.log());
  const std::string expectedStart = "action_log 1\n"
                                    "game dice_race 1\n"
                                    "start 4\n"
                                    "scores 0 0\npot 0\nturn 0\nwinner -1\n"
                                    "a roll 4\n"
                                    "a roll 3\n"
                                    "f 2 ";
  EXPECT_EQ(text.substr(0, expectedStart.size()), expectedStart);
  EXPECT_TRUE(text.ends_with("\na hold\n")) << text;
}

// Comments and blank lines (notes made when analysing) are skipped.
TEST(ActionLogTest, CommentsAreSkipped) {
  const std::string text = "# a game from Sunday\naction_log 1\ngame dice_race 1\n"
                           "start 4\nscores 0 0\npot 0\nturn 0\nwinner -1\n"
                           "\n# a big pot\na roll 6\na hold\n";
  const auto log = ActionLog::read<DiceRace>(text);
  ASSERT_TRUE(log.has_value()) << log.error().why;
  EXPECT_EQ(log->actions.size(), 2u);
  EXPECT_EQ(ActionLog::replay(*log).state.scores[0], 6);
}

// A changed roll makes a different game: the next fingerprint catches it
// (or the change makes an action illegal), and says where.
TEST(ActionLogTest, AnEditedGameIsCaught) {
  const Play play = playAGame(3);
  ActionLog::Log<DiceRace> edited = play.log();
  for (DiceRace::Action &action : edited.actions) {
    if (DiceRace::Roll *roll = std::get_if<DiceRace::Roll>(&action); roll && roll->face != 1) {
      roll->face = roll->face == 6 ? 5 : roll->face + 1;
      break;
    }
  }
  const ActionLog::Replay<DiceRace> result = ActionLog::replay(edited);
  ASSERT_FALSE(result.ok());
  EXPECT_LT(result.played, edited.actions.size());
}

TEST(ActionLogTest, OtherRulesVersionsSaySo) {
  ActionLog::Log<DiceRace> log = playAGame(5).log();
  log.rulesVersion = 0;
  const ActionLog::Replay<DiceRace> result = ActionLog::replay(log);
  ASSERT_FALSE(result.ok());
  EXPECT_NE(result.problem.value_or(ActionLog::Problem{}).why.find("rules version 0"),
            std::string::npos);
  EXPECT_EQ(result.played, 0u);
}

TEST(ActionLogTest, WhatIsntALogIsRefusedWithItsLine) {
  EXPECT_FALSE(ActionLog::read<DiceRace>("hello\n").has_value());
  EXPECT_FALSE(ActionLog::read<DiceRace>("action_log 1\ngame chess 1\n").has_value());
  const auto badAction = ActionLog::read<DiceRace>(
      "action_log 1\ngame dice_race 1\nstart 4\nscores 0 0\npot 0\nturn 0\nwinner -1\n"
      "a roll 4\na fly\n");
  ASSERT_FALSE(badAction.has_value());
  EXPECT_EQ(badAction.error().line, 9u); // "a fly"
  const auto badStart =
      ActionLog::read<DiceRace>("action_log 1\ngame dice_race 1\nstart 2\nscores 0 0\npot 0\n");
  ASSERT_FALSE(badStart.has_value());
  EXPECT_EQ(badStart.error().line, 4u);
}

// The replay can be watched as it goes, for analysis.
TEST(ActionLogTest, AReplayCanBeWatched) {
  const Play play = playAGame(9);
  int holds = 0;
  std::size_t seen = 0;
  ActionLog::replay(
      play.log(), [&](const DiceRace::State &, const DiceRace::Action &action, std::size_t index) {
        EXPECT_EQ(index, seen++);
        holds += std::holds_alternative<DiceRace::Hold>(action);
      });
  EXPECT_EQ(seen, play.log().actions.size());
  EXPECT_GT(holds, 0);
}
