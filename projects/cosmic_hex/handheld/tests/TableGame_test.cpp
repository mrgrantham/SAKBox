// The game at the table: bots and the dealer play a whole game; a person's
// move waits for them, and only a legal one is taken
#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>
#include <CosmicHex/Handheld/TradeArrival.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

TEST(TableGameTest, FourBotsPlayAGame) {
  TableGame game({.botIterations = 10, .seed = 3});
  for (int moves = 0; !game.over() && moves < 20000; ++moves) {
    ASSERT_FALSE(game.waitingForPerson());
    ASSERT_TRUE(game.step().has_value());
  }
  EXPECT_TRUE(game.over());
  EXPECT_EQ(game.history().size(), game.log().actions.size());
  // The log replays to the same end (as the lab would)
  EXPECT_TRUE(ActionLog::replay(game.log()).ok());
}

TEST(TableGameTest, APersonsMoveWaitsForThem) {
  TableGame game(
      {.seats = {Seat::Person, Seat::Person, Seat::Person, Seat::Person}, .botIterations = 10});
  // The dealer goes on by itself until it's someone's move
  while (!game.waitingForPerson() && !game.over())
    ASSERT_TRUE(game.step().has_value());
  ASSERT_TRUE(game.waitingForPerson());
  EXPECT_FALSE(game.step().has_value());               // (theirs to make)
  EXPECT_FALSE(game.play({Action::Kind::CashBadges})); // (not legal now)
  const std::vector<Action> legal = Game::legalActions(game.state());
  ASSERT_FALSE(legal.empty());
  EXPECT_TRUE(game.play(legal.front()));
  EXPECT_FALSE(game.history().empty());
}

// A person playing a whole game against three bots, as the consoles' move
// list offers: whenever it's their move there's at least one, any of them
// is taken, and the game goes on to its end (no phase leaves a person stuck)
TEST(TableGameTest, APersonCanPlayAWholeGame) {
  for (std::uint64_t seed : {11u, 12u, 13u}) {
    TableGame game({.seats = {Seat::Person, Seat::Bot, Seat::Bot, Seat::Bot},
                    .botIterations = 8,
                    .seed = seed});
    SeededRandom::Generator choose(seed);
    int personMoves = 0;
    for (int moves = 0; !game.over() && moves < 40000; ++moves) {
      if (game.waitingForPerson()) {
        const std::vector<Action> legal = game.personMoves();
        ASSERT_FALSE(legal.empty()) << "a person with no move, seed " << seed;
        // (mostly ending the turn early would stall a game: pick at random)
        ASSERT_TRUE(game.play(legal[choose.below(static_cast<std::uint32_t>(legal.size()))]));
        ++personMoves;
      } else {
        ASSERT_TRUE(game.step().has_value()) << "stuck, seed " << seed;
      }
    }
    EXPECT_TRUE(game.over()) << "seed " << seed;
    EXPECT_GT(personMoves, 10);
  }
}

// In their trading step a person is offered the plain trades: one of what
// they hold for one of another (two for one where they can), as offers to
// anyone, which the rules take
TEST(TableGameTest, APersonCanOfferTrades) {
  TableGame game(
      {.seats = {Seat::Person, Seat::Bot, Seat::Bot, Seat::Bot}, .botIterations = 8, .seed = 21});
  for (int steps = 0; steps < 20000 && !game.over(); ++steps) {
    if (game.waitingForPerson() && game.state().phase == Phase::Trading) {
      const std::vector<Action> offered = game.personMoves();
      const auto offer = std::find_if(offered.begin(), offered.end(), [](const Action &a) {
        return a.kind == Action::Kind::OfferTrade;
      });
      if (offer == offered.end()) { // (nothing to give yet)
        ASSERT_TRUE(game.play({Action::Kind::EndTrading}));
      } else {
        ASSERT_TRUE(game.play(*offer));
        EXPECT_EQ(game.state().phase, Phase::TradeReply);
        return;
      }
    } else if (game.waitingForPerson()) {
      ASSERT_TRUE(game.play(game.personMoves().back()));
    } else {
      ASSERT_TRUE(game.step().has_value());
    }
  }
  ADD_FAILURE() << "never had anything to offer";
}

// A table's rules: the agreed rules on its board, with its play
// style; every board on offer plays a game through
TEST(TableGameTest, EveryBoardAndPlayStylePlays) {
  for (const NamedBoard &choice : boardChoices()) {
    for (bool finishRound : {true, false}) {
      TableSettings settings{
          .botIterations = 5, .seed = 9, .board = choice.board, .finishRound = finishRound};
      EXPECT_EQ(rulesFor(settings).finishRound, finishRound);
      TableGame game(settings);
      EXPECT_EQ(game.state().settings.homeHexes, choice.board.homes) << choice.name;
      for (int moves = 0; moves < 20000 && !game.over(); ++moves) {
        if (!game.step()) {
          ADD_FAILURE() << choice.name << ": stuck";
          break;
        }
      }
      EXPECT_TRUE(game.over()) << choice.name;
    }
  }
}

// Physics dice: the game stops at each roll until the dice are
// given, and plays them; a whole game goes through with dice from outside
TEST(TableGameTest, PhysicsDiceWaitForTheThrow) {
  TableGame game(TableSettings{.botIterations = 5, .seed = 3, .physicsDice = true});
  int rolls = 0, given = 0;
  for (int moves = 0; moves < 20000 && !game.over(); ++moves) {
    if (game.waitingForDice()) {
      EXPECT_FALSE(game.step().has_value()); // (nothing moves until they're thrown)
      EXPECT_FALSE(game.rollDice(0, 3));     // (not a face)
      const int a = 1 + (given % 6), b = 1 + ((given / 6) % 6);
      ASSERT_TRUE(game.rollDice(a, b));
      ++given;
      EXPECT_EQ(game.state().lastRoll, a + b);
      ++rolls;
      continue;
    }
    if (!game.step()) {
      ADD_FAILURE() << "stuck";
      break;
    }
  }
  EXPECT_TRUE(game.over());
  EXPECT_GT(rolls, 10);
  EXPECT_FALSE(game.rollDice(3, 4)); // (over: not waiting)
}

// Who goes first (rollForFirst): the one rolling highest, those tied
// rolling again; the game starts with them; every player can be first
TEST(TableGameTest, TheHighestRollGoesFirst) {
  std::set<int> firsts;
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    SeededRandom::Generator random(seed);
    const FirstRoll roll = rollForFirst(random, 4);
    ASSERT_FALSE(roll.rounds.empty());
    const std::vector<int> &last = roll.rounds.back();
    for (int p = 0; p < 4; ++p) {
      if (p != roll.first) {
        EXPECT_LT(last[static_cast<std::size_t>(p)], last[static_cast<std::size_t>(roll.first)]);
      }
    }
    // Only those tied for the highest roll again
    for (std::size_t r = 1; r < roll.rounds.size(); ++r) {
      const auto &before = roll.rounds[r - 1];
      const int top = *std::max_element(before.begin(), before.end());
      for (std::size_t p = 0; p < 4; ++p) {
        EXPECT_EQ(roll.rounds[r][p] > 0, before[p] == top);
      }
    }
    firsts.insert(roll.first);
  }
  EXPECT_EQ(firsts.size(), 4u);
  TableSettings settings;
  settings.seed = 5;
  const TableGame game(settings);
  EXPECT_EQ(game.state().first, game.firstRoll().first);
}

// Every move made is kept as made, whose turn it was with it, in step with
// the words
TEST(TableGameTest, MovesAreKeptAsMade) {
  TableSettings settings;
  settings.seed = 5;
  settings.botIterations = 5;
  TableGame game(settings);
  for (int i = 0; i < 40 && game.step(); ++i) {
  }
  ASSERT_FALSE(game.played().empty());
  EXPECT_EQ(game.played().size(), game.history().size());
  for (const TableGame::Played &move : game.played()) {
    EXPECT_GE(move.player, 0);
    EXPECT_LT(move.player, settings.players);
  }
}

namespace {
// A person against three bots, their moves at random from `choose`, but
// `trading` looked at first on each of theirs (it may play one): true once
// `trading` says it's done, false if the game ends or stalls first
template <typename Trading>
bool playUntil(TableGame &game, SeededRandom::Generator &choose, Trading trading) {
  for (int steps = 0; steps < 40000 && !game.over(); ++steps) {
    if (game.waitingForPerson()) {
      if (trading())
        return true;
      if (!game.waitingForPerson())
        continue; // (`trading` played their move)
      const std::vector<Action> legal = game.personMoves();
      if (!game.play(legal[choose.below(static_cast<std::uint32_t>(legal.size()))]))
        return false;
    } else if (!game.step()) {
      return false;
    }
  }
  return false;
}
} // namespace

// A person counters a bot's offer on the touch panel (Counter, then the
// builder's Counter with the offer turned round), the rules take it, and the
// bot answers it; the game goes on
TEST(TableGameTest, APersonCountersABotsOffer) {
  bool countered = false;
  for (std::uint64_t seed = 1; seed <= 12 && !countered; ++seed) {
    TableGame game({.seats = {Seat::Person, Seat::Bot, Seat::Bot, Seat::Bot},
                    .botIterations = 6,
                    .seed = seed});
    SeededRandom::Generator choose(seed);
    BottomScreen screen(256, 192);
    const auto counter = [&] {
      if (game.state().phase != Phase::TradeReply)
        return false;
      screen.update(game.state(), game.personMoves(), true);
      if (screen.mode() != BottomScreen::Mode::Trade || !screen.trade().showingOffer())
        return false;
      screen.tap(30, 178, std::nullopt); // (Counter, in Back's place)
      EXPECT_TRUE(screen.trade().countering());
      // (holding none of what they asked for, there's nothing to give back:
      // Back to the offer, and on to the next)
      if (!screen.trade().whyNot().empty()) {
        screen.press(BottomScreen::Key::Back);
        EXPECT_TRUE(screen.trade().showingOffer());
        return false;
      }
      const BottomScreen::Outcome sent = screen.press(BottomScreen::Key::Confirm);
      EXPECT_TRUE(sent.play && sent.play->kind == Action::Kind::CounterTrade);
      return sent.play && game.play(*sent.play);
    };
    if (!playUntil(game, choose, counter))
      continue;
    countered = true;
    EXPECT_EQ(game.state().phase, Phase::TradeCounter);
    const std::optional<Action> answer = game.step(); // (the bot's)
    ASSERT_TRUE(answer.has_value());
    EXPECT_TRUE(answer->kind == Action::Kind::AcceptTrade ||
                answer->kind == Action::Kind::DeclineTrade);
    EXPECT_NE(game.state().phase, Phase::TradeCounter);
  }
  EXPECT_TRUE(countered) << "no offer to the person to counter";
}

// A bot counters a person's offer it won't take; the person is shown it to
// answer, announced as a counter (its banner, the screen arriving), and
// can't counter it again
TEST(TableGameTest, ABotCountersAPersonsOffer) {
  bool countered = false;
  for (std::uint64_t seed = 1; seed <= 12 && !countered; ++seed) {
    TableGame game({.seats = {Seat::Person, Seat::Bot, Seat::Bot, Seat::Bot},
                    .botIterations = 6,
                    .seed = seed});
    SeededRandom::Generator choose(seed);
    const auto offerUntilCountered = [&] {
      if (game.state().phase == Phase::TradeCounter)
        return true;
      if (game.state().phase != Phase::Trading)
        return false;
      // (each of the plain offers in turn: the first not yet made; then on)
      const std::vector<Action> moves = game.personMoves();
      const auto offer = std::find_if(moves.begin(), moves.end(), [&](const Action &move) {
        return move.kind == Action::Kind::OfferTrade &&
               std::none_of(game.state().offersMade.begin(), game.state().offersMade.end(),
                            [&](const std::array<int, 2> &made) {
                              return made[0] == move.a && made[1] == move.b;
                            });
      });
      EXPECT_TRUE(game.play(offer != moves.end() ? *offer : Action{Action::Kind::EndTrading}));
      return false;
    };
    if (!playUntil(game, choose, offerUntilCountered))
      continue;
    countered = true;
    ASSERT_EQ(game.state().phase, Phase::TradeCounter);
    ASSERT_TRUE(game.waitingForPerson());
    TradeRoom room;
    room.setNames(PlayerNames::forGame(4, 0, "James", seed));
    room.update(game.state(), game.personMoves(), true);
    ASSERT_TRUE(room.answering());
    EXPECT_TRUE(room.counterToYours());
    EXPECT_FALSE(room.canCounter());
    EXPECT_NE(room.headline().find(" COUNTERS"), std::string::npos);
    TradeArrival arrival;
    EXPECT_TRUE(arrival.saw(room.answering(), 0)); // (it arrives as an offer does)
    ASSERT_TRUE(room.decline().has_value());
    EXPECT_TRUE(game.play(*room.decline()));
  }
  EXPECT_TRUE(countered) << "no bot countered the person";
}
