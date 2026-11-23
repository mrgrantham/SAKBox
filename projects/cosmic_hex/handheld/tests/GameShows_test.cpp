// The stage's shows (GameShows), driven directly with snapshots on a clock
// of the test's own: the table held from the move that brings a show until
// it has shown, one show over the board at a time, the dice after the shows
// before them, banners queuing on one band, and B ending what animates but
// not what's still to come (the README's "Every show is seen whole")
#include <CosmicHex/Handheld/GameShows.hpp>

#include "TestTables.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Kind = Action::Kind;
using Hold = GameShows::Hold;

namespace {

constexpr double kFrame = 1.0 / 60;
constexpr std::uint64_t kSeed = 1;

PlayerNames namesOf() { return PlayerNames::forGame(4, -1, "", kSeed); }

// A game in its main part, `turn`'s turn, as the screens get it
State mainPart(int turn) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  state.phase = Phase::Main;
  state.turn = turn;
  return state;
}

// The shows of a watched game, on the test's clock: each show logged, and
// each time the table is told to hold or go on kept
class Stage {
public:
  Stage() : shows(256, timeAt(0)) {
    shows.setLog(&log);
    shows.setHooks(
        {.hold = [this](bool held) { holds.push_back(held); },
         .play = [this](const Action &move) { played.push_back(move); },
         .diceRolled = [this](int first, int second) { rolled.emplace_back(first, second); },
         .closeUpTile = {}});
    shows.startGame(false, namesOf(), false);
  }
  // (its hooks hold on to it: it stays where it's made)
  Stage(const Stage &) = delete;
  Stage &operator=(const Stage &) = delete;

  void arrive(const TableSnapshot &snapshot) {
    shows.setNow(timeAt(now));
    shows.arrive(snapshot);
  }
  void frame() {
    now += kFrame;
    shows.setNow(timeAt(now));
    shows.frame();
  }
  void framesFor(double seconds) {
    for (const double end = now + seconds; now < end;)
      frame();
  }
  // Frames until `done` says so (at most `seconds`): whether it did
  bool framesUntil(const std::function<bool()> &done, double seconds = 20) {
    for (const double end = now + seconds; now < end; frame())
      if (done())
        return true;
    return done();
  }
  // The log's shows of `kind`, ended (every show still open closed first)
  std::vector<ShowLog::Entry> entries(ShowKind kind) {
    log.finish(now);
    std::vector<ShowLog::Entry> found;
    for (const ShowLog::Entry &entry : log.entries())
      if (entry.kind == kind)
        found.push_back(entry);
    return found;
  }

  GameShows shows;
  ShowLog log;
  std::vector<bool> holds;
  std::vector<Action> played;
  std::vector<std::pair<int, int>> rolled;
  double now = 0;

private:
  static GameShows::RealClock::time_point timeAt(double seconds) {
    return GameShows::RealClock::time_point{} +
           std::chrono::duration_cast<GameShows::RealClock::duration>(
               std::chrono::duration<double>(seconds));
  }
};

const TableGame::Played kEnforcerPlayed{1, {Kind::PlayEnforcer}};
const TableGame::Played kHomePaysMinerals{
    1, {Kind::RollHomeDie, static_cast<int>(Resource::Minerals)}};

// How many shows over the board are moving now (a discovery, a home's pay
// out for the game, a card played)
int showsOverTheBoard(const GameShows &shows) {
  const double t = shows.showsNow();
  const bool home = shows.homePay().moving(t) && !shows.homePay().waitingForGame(t);
  return (shows.discovery().moving(t) ? 1 : 0) + (home ? 1 : 0) +
         (shows.cardPlay().moving(t) ? 1 : 0);
}

} // namespace

// The table is held the moment a move brings a show, before any frame of
// it, and let go once it has shown: told once each way
TEST(GameShowsTest, TheTableIsHeldFromTheMoveUntilItsShowHasShown) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.framesFor(0.2);
  EXPECT_FALSE(stage.shows.holding());
  EXPECT_TRUE(stage.holds.empty());
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed}));
  EXPECT_TRUE(stage.shows.held(Hold::CardPlay)); // (at once: the table's next move waits)
  EXPECT_EQ(stage.holds, std::vector<bool>{true});
  const double heldFrom = stage.now;
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.holding(); }));
  EXPECT_GE(stage.now - heldFrom, CardPlayShow::kSeconds);
  EXPECT_EQ(stage.holds, (std::vector<bool>{true, false}));
  const auto plays = stage.entries(ShowKind::CardPlay);
  ASSERT_EQ(plays.size(), 1u);
  EXPECT_EQ(plays[0].cut(), ShowLog::Cut::None);
}

// The board's pieces moving hold the table too, and your move made while
// they fly is sent once they've landed
TEST(GameShowsTest, YourMoveWaitsForThePiecesToLand) {
  Stage stage;
  State before = mainPart(1);
  before.ships = {Ship{1, Hex{}, false, Corner{{0, 0}, 0}}};
  State after = before;
  after.ships[0].corner = Corner{{1, 0}, 1};
  stage.arrive(TestTables::snapshotWith(before));
  stage.frame();
  stage.arrive(TestTables::snapshotWith(after));
  EXPECT_TRUE(stage.shows.piecesMoving());
  EXPECT_TRUE(stage.shows.held(Hold::Stage));
  stage.shows.play({Kind::EndTurn});
  EXPECT_TRUE(stage.played.empty()); // (not while they fly)
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.piecesMoving(); }));
  stage.frame();
  EXPECT_EQ(stage.played, std::vector<Action>{Action{Kind::EndTurn}});
  EXPECT_FALSE(stage.shows.holding());
  EXPECT_EQ(stage.holds, (std::vector<bool>{true, false}));
}

// A turn's banner and the card played with it: the card waits for the
// banner, and the table is held through both as one hold
TEST(GameShowsTest, ABannerAndTheShowAfterItAreOneHold) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  stage.arrive(TestTables::snapshotWith(mainPart(2), {kEnforcerPlayed}));
  EXPECT_TRUE(stage.shows.banner().moving(stage.shows.gameNow()));
  EXPECT_TRUE(stage.shows.held(Hold::Stage));
  EXPECT_TRUE(stage.shows.held(Hold::CardPlay));
  bool cardSeen = false;
  ASSERT_TRUE(stage.framesUntil([&] {
    if (stage.shows.cardPlay().moving(stage.shows.showsNow())) {
      cardSeen = true;
      EXPECT_FALSE(stage.shows.banner().showing(stage.shows.gameNow())) << "at " << stage.now;
    }
    return !stage.shows.holding();
  }));
  EXPECT_TRUE(cardSeen);
  EXPECT_EQ(stage.holds, (std::vector<bool>{true, false}));
  const auto banners = stage.entries(ShowKind::Banner);
  const auto plays = stage.entries(ShowKind::CardPlay);
  ASSERT_EQ(banners.size(), 1u);
  ASSERT_EQ(plays.size(), 1u);
  EXPECT_EQ(banners[0].cut(), ShowLog::Cut::None);
  EXPECT_EQ(plays[0].cut(), ShowLog::Cut::None);
}

// A home's pay that comes while a card played has the stage waits its turn:
// never two shows over the board at once, each seen whole, in the order
// they came
TEST(GameShowsTest, ShowsTakeTheStageInTurn) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed}));
  stage.framesFor(0.3);
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kHomePaysMinerals}));
  EXPECT_TRUE(stage.shows.held(Hold::HomePay)); // (held for it already, still to come)
  std::optional<double> cardLastMoved, homeFirstMoved;
  ASSERT_TRUE(stage.framesUntil([&] {
    EXPECT_LE(showsOverTheBoard(stage.shows), 1) << "at " << stage.now;
    const double t = stage.shows.showsNow();
    if (stage.shows.cardPlay().moving(t))
      cardLastMoved = stage.now;
    if (stage.shows.homePay().moving(t) && !homeFirstMoved)
      homeFirstMoved = stage.now;
    return !stage.shows.holding();
  }));
  ASSERT_TRUE(cardLastMoved && homeFirstMoved);
  EXPECT_GT(*homeFirstMoved, *cardLastMoved);
  const auto plays = stage.entries(ShowKind::CardPlay);
  const auto pays = stage.entries(ShowKind::HomePay);
  ASSERT_EQ(plays.size(), 1u);
  ASSERT_EQ(pays.size(), 1u);
  EXPECT_EQ(plays[0].cut(), ShowLog::Cut::None);
  EXPECT_EQ(pays[0].cut(), ShowLog::Cut::None);
}

// The dice wait for the shows before them, a card played and a home's pay
// still to come after it, and are thrown once both have shown
TEST(GameShowsTest, TheDiceWaitForTheShowsBeforeThem) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  TableSnapshot rolling =
      TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed, kHomePaysMinerals});
  rolling.waitingForDice = true;
  rolling.diceSeed = 7;
  stage.arrive(rolling);
  ASSERT_TRUE(stage.framesUntil([&] {
    const bool showsBefore =
        stage.shows.cardPlay().current() != nullptr || stage.shows.homePay().current() != nullptr;
    if (showsBefore)
      EXPECT_TRUE(stage.shows.roll().idle()) << "the dice thrown under a show, at " << stage.now;
    return !stage.shows.roll().idle();
  }));
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.rolled.empty(); }));
  EXPECT_EQ(stage.rolled.size(), 1u);
  for (const int face : {stage.rolled[0].first, stage.rolled[0].second}) {
    EXPECT_GE(face, 1);
    EXPECT_LE(face, 6);
  }
}

// A card played while the dice are out waits for them to go: thrown, they're
// seen until they've gone
TEST(GameShowsTest, AShowWaitsForTheDiceOut) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  TableSnapshot rolling = TestTables::snapshotWith(mainPart(1));
  rolling.waitingForDice = true;
  rolling.diceSeed = 7;
  stage.arrive(rolling);
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.roll().idle(); }));
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed}));
  bool cardSeen = false;
  ASSERT_TRUE(stage.framesUntil([&] {
    const bool card = stage.shows.cardPlay().moving(stage.shows.showsNow());
    if (card)
      EXPECT_TRUE(stage.shows.roll().idle()) << "a card played over the dice, at " << stage.now;
    cardSeen = cardSeen || card;
    return cardSeen && !stage.shows.holding();
  }));
  EXPECT_EQ(stage.rolled.size(), 1u);
}

// A banner while another sweeps follows it on the same band, never cutting
// it short; the table held through both as one hold
TEST(GameShowsTest, ABannerWhileAnotherSweepsFollowsOnItsBand) {
  Stage stage;
  const PlayerNames names = namesOf();
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  stage.arrive(TestTables::snapshotWith(mainPart(2)));
  const auto bandStarted = stage.shows.banner().startedAt();
  ASSERT_TRUE(bandStarted.has_value());
  stage.framesFor(1);
  stage.arrive(TestTables::snapshotWith(mainPart(3)));
  EXPECT_EQ(stage.shows.banner().startedAt(), bandStarted); // (the same band, not a new one)
  // The first's words held their full time, then the second's
  std::vector<std::string> titles;
  ASSERT_TRUE(stage.framesUntil([&] {
    if (const auto look = stage.shows.banner().at(stage.shows.gameNow()))
      if (titles.empty() || titles.back() != look->title)
        titles.push_back(look->title);
    return !stage.shows.holding();
  }));
  EXPECT_EQ(titles,
            (std::vector<std::string>{names.upperOf(2) + "'S TURN", names.upperOf(3) + "'S TURN"}));
  EXPECT_GE(stage.now - *bandStarted, 2 * ScratchPadd::Banner::kHold);
  EXPECT_EQ(stage.holds, (std::vector<bool>{true, false}));
  for (const ShowLog::Entry &entry : stage.entries(ShowKind::Banner))
    EXPECT_EQ(entry.cut(), ShowLog::Cut::None);
}

// A banner that comes while a show plays waits for it (drawn under it, it
// swept by unseen), the table held meanwhile
TEST(GameShowsTest, ABannerWhileAShowPlaysWaitsForIt) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed}));
  stage.framesFor(0.5);
  ASSERT_TRUE(stage.shows.cardPlay().moving(stage.shows.showsNow()));
  stage.arrive(TestTables::snapshotWith(mainPart(2)));
  EXPECT_FALSE(stage.shows.banner().showing(stage.shows.gameNow()));
  EXPECT_TRUE(stage.shows.held(Hold::Stage)); // (a banner waiting holds the table too)
  ASSERT_TRUE(stage.framesUntil([&] {
    if (stage.shows.cardPlay().moving(stage.shows.showsNow()))
      EXPECT_FALSE(stage.shows.banner().showing(stage.shows.gameNow()));
    return !stage.shows.holding();
  }));
  const auto plays = stage.entries(ShowKind::CardPlay);
  const auto banners = stage.entries(ShowKind::Banner);
  ASSERT_EQ(plays.size(), 1u);
  ASSERT_EQ(banners.size(), 1u);
  EXPECT_GE(banners[0].startedAt, plays[0].endedAt - kFrame);
  EXPECT_EQ(plays[0].cut(), ShowLog::Cut::None);
  EXPECT_EQ(banners[0].cut(), ShowLog::Cut::None);
}

// B ends what's animating, not what's to come: the turn's banner skipped,
// the home's pay waiting for it starts afresh after, seen whole
TEST(GameShowsTest, SkipEndsWhatAnimatesNotWhatsToCome) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  stage.arrive(TestTables::snapshotWith(mainPart(2), {kHomePaysMinerals}));
  stage.framesFor(0.5);
  ASSERT_TRUE(stage.shows.skippable());
  ASSERT_FALSE(stage.shows.homePay().moving(stage.shows.showsNow())); // (behind the banner)
  stage.shows.skip(true);
  EXPECT_FALSE(stage.shows.banner().moving(stage.shows.gameNow()));
  ASSERT_NE(stage.shows.homePay().current(), nullptr); // (still to come, not skipped)
  EXPECT_GE(stage.shows.homePay().outAt(), stage.shows.showsNow());
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.holding(); }));
  const auto banners = stage.entries(ShowKind::Banner);
  const auto pays = stage.entries(ShowKind::HomePay);
  ASSERT_EQ(banners.size(), 1u);
  ASSERT_EQ(pays.size(), 1u);
  EXPECT_EQ(banners[0].cut(), ShowLog::Cut::SkipPressed);
  EXPECT_EQ(pays[0].cut(), ShowLog::Cut::None);
  EXPECT_GE(pays[0].movingSeconds, HomePayShow::seconds(1) - 2 * kFrame);
}

// Another card played, queued behind the one B skipped, is still to come:
// it starts afresh after it, seen whole
TEST(GameShowsTest, SkipLeavesTheNextCardPlayedToCome) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  const TableGame::Played cartel{2, {Kind::PlayCartel}};
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed, cartel}));
  stage.framesFor(0.5);
  stage.shows.skip(true);
  stage.frame();
  ASSERT_NE(stage.shows.cardPlay().current(), nullptr);
  EXPECT_EQ(stage.shows.cardPlay().current()->player, 2);
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.holding(); }));
  const auto plays = stage.entries(ShowKind::CardPlay);
  ASSERT_EQ(plays.size(), 2u);
  EXPECT_EQ(plays[0].cut(), ShowLog::Cut::SkipPressed);
  EXPECT_GE(plays[1].seenSeconds, CardPlayShow::kSeconds - 2 * kFrame);
  EXPECT_EQ(plays[1].lateBy, 0);
}

// The log: the next card played, starting in the frame after B ended the
// one before, is a show of its own, played whole, not the skipped one moved
// on in place (it was logged "skip pressed", as though the player had
// skipped it too)
TEST(GameShowsTest, TheNextCardPlayedAfterBIsLoggedAsPlayed) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  const TableGame::Played cartel{2, {Kind::PlayCartel}};
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed, cartel}));
  stage.framesFor(0.5);
  stage.shows.skip(true);
  stage.frame(); // (the next one starts in this frame)
  ASSERT_TRUE(stage.shows.cardPlay().moving(stage.shows.showsNow()));
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.holding(); }));
  const auto plays = stage.entries(ShowKind::CardPlay);
  ASSERT_EQ(plays.size(), 2u);
  EXPECT_EQ(plays[0].cut(), ShowLog::Cut::SkipPressed);
  EXPECT_EQ(plays[1].cut(), ShowLog::Cut::None);
  EXPECT_FALSE(plays[1].skipPressed);
}

// ...while a banner B moves on to the prompt it waits for is the same show,
// skipped in place (its prompt's words already in, not a new one late)
TEST(GameShowsTest, ABannerMovedOnToItsPromptByBIsLoggedAsSkipped) {
  ShowLog log;
  const auto banner = [](double key, double elapsed) {
    return ShowSeen{.kind = ShowKind::Banner,
                    .on = true,
                    .moving = true,
                    .visible = true,
                    .key = key,
                    .elapsed = elapsed};
  };
  const ShowSeen first = banner(0, 0);
  log.frame(0, kFrame, {&first, 1}, ShowKind::Banner);
  const ShowSeen sweeping = banner(0, 0.5);
  log.frame(0.5, kFrame, {&sweeping, 1}, ShowKind::Banner);
  log.skipped(true);
  const ShowSeen prompt = banner(0.6 - 0.55, 0.55);
  log.frame(0.6, kFrame, {&prompt, 1}, ShowKind::Banner);
  log.finish(1);
  ASSERT_EQ(log.entries().size(), 2u);
  EXPECT_EQ(log.entries()[1].cut(), ShowLog::Cut::SkipPressed);
  EXPECT_EQ(log.entries()[1].lateBy, 0);
}

// Another player's home pay queued behind the one B skips is still to come:
// it starts afresh after it, seen whole (HomePayShow timed the next home's
// die from the skipped one's end, long past on the jumped clock: never seen)
TEST(GameShowsTest, SkipLeavesTheNextHomesPayToCome) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  const TableGame::Played nextHome{2, {Kind::RollHomeDie, static_cast<int>(Resource::Food)}};
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kHomePaysMinerals, nextHome}));
  stage.framesFor(0.5);
  ASSERT_TRUE(stage.shows.homePay().moving(stage.shows.showsNow()));
  stage.shows.skip(true);
  stage.frame();
  ASSERT_NE(stage.shows.homePay().current(), nullptr);
  EXPECT_EQ(stage.shows.homePay().current()->player, 2);
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.holding(); }));
  const auto pays = stage.entries(ShowKind::HomePay);
  ASSERT_EQ(pays.size(), 2u);
  EXPECT_EQ(pays[1].cut(), ShowLog::Cut::None);
  EXPECT_GE(pays[1].seenSeconds, HomePayShow::seconds(1) - 2 * kFrame);
}

// A home's pay due the moment it came, B pressed before a frame of it was
// drawn (an impatient player, in the sweeps): it hadn't been seen, so it's
// still to come, and starts afresh after (it was jumped past, never seen)
TEST(GameShowsTest, SkipBeforeAShowsFirstFrameLeavesItToCome) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.framesFor(0.2);
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kHomePaysMinerals}));
  ASSERT_TRUE(stage.shows.homePay().moving(stage.shows.showsNow())); // (due now)
  stage.shows.skip(true);
  ASSERT_NE(stage.shows.homePay().current(), nullptr);
  EXPECT_GE(stage.shows.homePay().outAt(), stage.shows.showsNow());
  ASSERT_TRUE(stage.framesUntil([&] { return !stage.shows.holding(); }));
  const auto pays = stage.entries(ShowKind::HomePay);
  ASSERT_EQ(pays.size(), 1u);
  EXPECT_EQ(pays[0].cut(), ShowLog::Cut::None);
  EXPECT_GE(pays[0].seenSeconds, HomePayShow::seconds(1) - 2 * kFrame);
}

// ...and what is animating goes straight to its end
TEST(GameShowsTest, SkipTakesAShowPlayingToItsEnd) {
  Stage stage;
  stage.arrive(TestTables::snapshotWith(mainPart(1)));
  stage.frame();
  stage.arrive(TestTables::snapshotWith(mainPart(1), {kEnforcerPlayed}));
  stage.framesFor(0.5);
  ASSERT_TRUE(stage.shows.skippable());
  stage.shows.skip(true);
  EXPECT_FALSE(stage.shows.cardPlay().moving(stage.shows.showsNow()));
  stage.frame();
  EXPECT_FALSE(stage.shows.holding());
  const auto plays = stage.entries(ShowKind::CardPlay);
  ASSERT_EQ(plays.size(), 1u);
  EXPECT_EQ(plays[0].cut(), ShowLog::Cut::SkipPressed);
}
