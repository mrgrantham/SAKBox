// The bots: the random bot plays only allowed moves; ISMCTS finds wins and
// blocks, beats random play in a game of chance, doesn't peek at hidden
// information, makes mistakes when told to, and is repeatable; and whole
// matches are recorded in action logs that replay.
#include "TestGames.hpp"

#include <GameBots/Bots.hpp>
#include <SeededRandom/Seed.hpp>

#include <gtest/gtest.h>

#include <algorithm>

using TestGames::DiceRace;
using TestGames::HiddenNumber;
using TestGames::TicTacToe;

namespace {

// Squares written as a string, X and O, '.' for empty; whose turn it is
// follows from the counts
TicTacToe::State board(std::string_view squares) {
  TicTacToe::State state;
  int pieces = 0;
  for (int i = 0; i < 9; ++i) {
    state.squares[static_cast<std::size_t>(i)] = squares[static_cast<std::size_t>(i)];
    pieces += squares[static_cast<std::size_t>(i)] != '.';
  }
  state.turn = pieces % 2;
  return state;
}

// Plays a match and returns the winner (-1 for a draw)
template <typename G> int winnerOf(const ActionLog::Play<G> &play);
template <> int winnerOf(const ActionLog::Play<TicTacToe> &play) {
  return TicTacToe::winner(play.state());
}
template <> int winnerOf(const ActionLog::Play<DiceRace> &play) {
  return DiceRace::winner(play.state());
}

} // namespace

TEST(BotsTest, TheRandomBotPlaysAllowedMoves) {
  GameBots::RandomBot<TicTacToe> bot;
  SeededRandom::Generator random(1);
  const TicTacToe::State state = board("XO.X.O...");
  const std::vector<int> legal = TicTacToe::legalActions(state);
  for (int i = 0; i < 100; ++i)
    EXPECT_NE(std::find(legal.begin(), legal.end(), bot.choose(state, random)), legal.end());
}

// X to play can win at square 6 (the left column)
TEST(BotsTest, IsmctsTakesAWin) {
  GameBots::Ismcts<TicTacToe> bot;
  SeededRandom::Generator random(2);
  EXPECT_EQ(bot.choose(board("XOOX.....") /* X to play */, random, {.iterations = 2000}), 6);
}

// O to play must block X's diagonal at square 8
TEST(BotsTest, IsmctsBlocksALoss) {
  GameBots::Ismcts<TicTacToe> bot;
  SeededRandom::Generator random(3);
  EXPECT_EQ(bot.choose(board("XO..X...."), random, {.iterations = 2000}), 8);
}

// Against random play at noughts and crosses it never loses, whichever side
TEST(BotsTest, IsmctsNeverLosesToRandomPlay) {
  for (unsigned seed = 1; seed <= 20; ++seed) {
    const int botSide = static_cast<int>(seed % 2);
    SeededRandom::Seed s(seed);
    SeededRandom::Generator botRandom = s.stream("bot"), otherRandom = s.stream("other"),
                            dealer = s.stream("dealer");
    GameBots::Ismcts<TicTacToe> bot;
    GameBots::RandomBot<TicTacToe> other;
    std::vector<std::function<int(const TicTacToe::State &)>> players(2);
    players[static_cast<std::size_t>(botSide)] = [&](const TicTacToe::State &state) {
      return bot.choose(state, botRandom, {.iterations = 1000});
    };
    players[static_cast<std::size_t>(1 - botSide)] = [&](const TicTacToe::State &state) {
      return other.choose(state, otherRandom);
    };
    ActionLog::Play<TicTacToe> play(TicTacToe::State{});
    GameBots::playMatch(play, players, dealer);
    EXPECT_NE(winnerOf(play), 1 - botSide) << "seed " << seed;
  }
}

// In a game of chance it wins far more often than random play
TEST(BotsTest, IsmctsBeatsRandomPlayInAGameOfChance) {
  int botWins = 0;
  constexpr int kGames = 40;
  for (unsigned seed = 1; seed <= kGames; ++seed) {
    const int botSide = static_cast<int>(seed % 2);
    SeededRandom::Seed s(seed);
    SeededRandom::Generator botRandom = s.stream("bot"), otherRandom = s.stream("other"),
                            dealer = s.stream("dealer");
    GameBots::Ismcts<DiceRace> bot;
    GameBots::RandomBot<DiceRace> other;
    std::vector<std::function<DiceRace::Action(const DiceRace::State &)>> players(2);
    players[static_cast<std::size_t>(botSide)] = [&](const DiceRace::State &state) {
      return bot.choose(state, botRandom, {.iterations = 300});
    };
    players[static_cast<std::size_t>(1 - botSide)] = [&](const DiceRace::State &state) {
      return other.choose(state, otherRandom);
    };
    ActionLog::Play<DiceRace> play(DiceRace::State{});
    GameBots::playMatch(play, players, dealer);
    botWins += winnerOf(play) == botSide;
  }
  EXPECT_GE(botWins, kGames * 3 / 4) << botWins << " of " << kGames;
}

// The bot can't see the secret number: its guess is the same whatever the
// number is, and it wins about a third of the time, as a fair guesser does
TEST(BotsTest, IsmctsDoesntPeek) {
  GameBots::Ismcts<HiddenNumber> bot;
  for (unsigned seed = 1; seed <= 20; ++seed) {
    SeededRandom::Generator a(seed), b(seed);
    EXPECT_EQ(bot.choose(HiddenNumber::State{1, 0}, a, {.iterations = 200}),
              bot.choose(HiddenNumber::State{3, 0}, b, {.iterations = 200}));
  }
  int right = 0;
  constexpr int kGames = 300;
  SeededRandom::Generator random(7), dealer(8);
  for (int i = 0; i < kGames; ++i) {
    HiddenNumber::State state;
    HiddenNumber::apply(state, HiddenNumber::dealerAction(state, dealer));
    right += bot.choose(state, random, {.iterations = 50}) == state.secret;
  }
  EXPECT_NEAR(right, kGames / 3.0, kGames * 0.08);
}

// The same state, generator and budget give the same move
TEST(BotsTest, IsmctsIsRepeatable) {
  GameBots::Ismcts<DiceRace> bot;
  DiceRace::State state;
  state.scores = {12, 15};
  state.pot = 6;
  for (unsigned seed = 1; seed <= 10; ++seed) {
    SeededRandom::Generator a(seed), b(seed);
    EXPECT_EQ(bot.choose(state, a, {.iterations = 200}), bot.choose(state, b, {.iterations = 200}));
  }
}

// Told to make mistakes, it sometimes plays its second-best move: here it
// sometimes misses the win
TEST(BotsTest, MistakesCanBeAskedFor) {
  GameBots::Ismcts<TicTacToe> careful, careless({.mistakePercent = 50});
  int carefulMisses = 0, carelessMisses = 0;
  for (unsigned seed = 1; seed <= 40; ++seed) {
    SeededRandom::Generator a(seed), b(seed);
    carefulMisses += careful.choose(board("XOOX....."), a, {.iterations = 500}) != 6;
    carelessMisses += careless.choose(board("XOOX....."), b, {.iterations = 500}) != 6;
  }
  EXPECT_EQ(carefulMisses, 0);
  EXPECT_GT(carelessMisses, 10);
  EXPECT_LT(carelessMisses, 30);
}

// A time limit stops the search early
TEST(BotsTest, ATimeLimitStopsTheSearch) {
  GameBots::Ismcts<TicTacToe> bot;
  SeededRandom::Generator random(4);
  int checks = 0;
  const int move = bot.choose(board("XOOX....."), random,
                              {.iterations = 1000000, .outOfTime = [&] { return ++checks > 50; }});
  EXPECT_EQ(checks, 51);
  EXPECT_GE(move, 0);
}

// A simulated match is an ordinary action log: it replays to the same end
TEST(BotsTest, MatchesAreRecordedAndReplay) {
  SeededRandom::Seed seed(42);
  SeededRandom::Generator one = seed.stream("one"), two = seed.stream("two"),
                          dealer = seed.stream("dealer");
  GameBots::Ismcts<DiceRace> bot;
  GameBots::RandomBot<DiceRace> other;
  ActionLog::Play<DiceRace> play(DiceRace::State{});
  GameBots::playMatch<DiceRace>(
      play,
      {[&](const DiceRace::State &s) { return bot.choose(s, one, {.iterations = 100}); },
       [&](const DiceRace::State &s) { return other.choose(s, two); }},
      dealer);
  EXPECT_TRUE(DiceRace::isOver(play.state()));
  const ActionLog::Replay<DiceRace> again = ActionLog::replay(play.log());
  EXPECT_TRUE(again.ok());
  EXPECT_EQ(again.state, play.state());
}

// A style (rolloutWeight) leans only the bot's own moves in its rollouts:
// it's asked about the bot's moves alone; it doesn't stop the bot taking a
// win; and weights that are all 0 are the same as none
TEST(BotsTest, AStyleLeansOnlyTheBotsOwnRolloutMoves) {
  const TicTacToe::State state = board("XOOX....." /* X to play */);
  int asked = 0, askedForOthers = 0;
  GameBots::Ismcts<TicTacToe> styled(
      {.rolloutWeight = [&](const TicTacToe::State &s, const int &move) {
        ++asked;
        if (TicTacToe::currentPlayer(s) != 0)
          ++askedForOthers;
        return move == 8 ? 50.0 : 1.0; // (fond of the far corner)
      }});
  SeededRandom::Generator random(2);
  EXPECT_EQ(styled.choose(state, random, {.iterations = 2000}), 6); // still takes the win
  EXPECT_GT(asked, 0);
  EXPECT_EQ(askedForOthers, 0);

  GameBots::Ismcts<TicTacToe> plain;
  GameBots::Ismcts<TicTacToe> allZero(
      {.rolloutWeight = [](const TicTacToe::State &, const int &) { return 0.0; }});
  for (unsigned seed = 1; seed <= 10; ++seed) {
    SeededRandom::Generator a(seed), b(seed);
    const TicTacToe::State opening = board(".........");
    EXPECT_EQ(plain.choose(opening, a, {.iterations = 300}),
              allZero.choose(opening, b, {.iterations = 300}));
  }
}

// Scoring in fixed point (Arithmetic::FixedPoint, round 92: for the DS) finds
// the same win and block, never loses to random play, and beats it at chance
namespace {
using Fixed = GameBots::Ismcts<TicTacToe>::Settings::Arithmetic;
} // namespace

TEST(BotsTest, FixedPointScoringPlaysAsWell) {
  {
    GameBots::Ismcts<TicTacToe> bot({.arithmetic = Fixed::FixedPoint});
    SeededRandom::Generator random(2);
    EXPECT_EQ(bot.choose(board("XOOX....."), random, {.iterations = 2000}), 6);
  }
  {
    GameBots::Ismcts<TicTacToe> bot({.arithmetic = Fixed::FixedPoint});
    SeededRandom::Generator random(3);
    EXPECT_EQ(bot.choose(board("XO..X...."), random, {.iterations = 2000}), 8);
  }
  for (unsigned seed = 1; seed <= 20; ++seed) {
    const int botSide = static_cast<int>(seed % 2);
    SeededRandom::Seed s(seed);
    SeededRandom::Generator botRandom = s.stream("bot"), otherRandom = s.stream("other"),
                            dealer = s.stream("dealer");
    GameBots::Ismcts<TicTacToe> bot({.arithmetic = Fixed::FixedPoint});
    GameBots::RandomBot<TicTacToe> other;
    std::vector<std::function<int(const TicTacToe::State &)>> players(2);
    players[static_cast<std::size_t>(botSide)] = [&](const TicTacToe::State &state) {
      return bot.choose(state, botRandom, {.iterations = 1000});
    };
    players[static_cast<std::size_t>(1 - botSide)] = [&](const TicTacToe::State &state) {
      return other.choose(state, otherRandom);
    };
    ActionLog::Play<TicTacToe> play(TicTacToe::State{});
    GameBots::playMatch(play, players, dealer);
    EXPECT_NE(winnerOf(play), 1 - botSide) << "seed " << seed;
  }
  int botWins = 0;
  constexpr int kGames = 40;
  for (unsigned seed = 1; seed <= kGames; ++seed) {
    const int botSide = static_cast<int>(seed % 2);
    SeededRandom::Seed s(seed);
    SeededRandom::Generator botRandom = s.stream("bot"), otherRandom = s.stream("other"),
                            dealer = s.stream("dealer");
    GameBots::Ismcts<DiceRace> bot(
        {.arithmetic = GameBots::Ismcts<DiceRace>::Settings::Arithmetic::FixedPoint});
    GameBots::RandomBot<DiceRace> other;
    std::vector<std::function<DiceRace::Action(const DiceRace::State &)>> players(2);
    players[static_cast<std::size_t>(botSide)] = [&](const DiceRace::State &state) {
      return bot.choose(state, botRandom, {.iterations = 300});
    };
    players[static_cast<std::size_t>(1 - botSide)] = [&](const DiceRace::State &state) {
      return other.choose(state, otherRandom);
    };
    ActionLog::Play<DiceRace> play(DiceRace::State{});
    GameBots::playMatch(play, players, dealer);
    botWins += winnerOf(play) == botSide;
  }
  EXPECT_GE(botWins, kGames * 3 / 4) << botWins << " of " << kGames;
}
