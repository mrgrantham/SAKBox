// An offer to you arriving: its banner, then the trade screen coming down
// the touch panel once the banner's words have settled (it used to just
// appear), at the Animations setting's pace, and in place at once on a skip
#include <CosmicHex/Handheld/SettingsScreen.hpp>
#include <CosmicHex/Handheld/TradeArrival.hpp>

#include <ScratchPadd/Banner/Banner.hpp>
#include <ScratchPadd/GameClock.hpp>
#include <ScratchPadd/Scene/Font.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;
using ScratchPadd::Banner;

namespace {
// When the banner's words have come in and settled, and when they start off
constexpr double kWordsSettled = Banner::kWordsInAfter + Banner::kWordsIn;
constexpr double kWordsLeave = kWordsSettled + Banner::kHold;
} // namespace

// Regression (the offer just appeared): the board stays until the banner's
// words settle, then the screen comes down, in place before they leave
TEST(TradeArrivalTest, TheScreenComesDownAfterTheBannersWordsSettle) {
  TradeArrival arrival;
  EXPECT_FALSE(arrival.saw(false, 0));
  EXPECT_FLOAT_EQ(arrival.down(0), 1); // (nothing arriving: as it is)
  ASSERT_TRUE(arrival.saw(true, 10));
  EXPECT_FLOAT_EQ(arrival.down(10), 0);
  EXPECT_FLOAT_EQ(arrival.down(10 + kWordsSettled - 0.01), 0);
  EXPECT_TRUE(arrival.moving(10 + kWordsSettled - 0.01));
  const float midway = arrival.down(10 + kWordsSettled + TradeArrival::kSeconds / 2);
  EXPECT_GT(midway, 0.5f); // (eased: quick at first, slowing to a stop)
  EXPECT_LT(midway, 1);
  const double inPlace = 10 + TradeArrival::kStartsAfter + TradeArrival::kSeconds;
  EXPECT_FLOAT_EQ(arrival.down(inPlace), 1);
  EXPECT_FALSE(arrival.moving(inPlace));
  EXPECT_LT(TradeArrival::kStartsAfter + TradeArrival::kSeconds, kWordsLeave);
}

// Once: the same answer seen again doesn't start it over, and a new one
// (after the last was answered) does
TEST(TradeArrivalTest, EachAnswerArrivesOnce) {
  TradeArrival arrival;
  ASSERT_TRUE(arrival.saw(true, 0));
  EXPECT_FALSE(arrival.saw(true, 0.2));
  EXPECT_FLOAT_EQ(arrival.down(0.2), 0); // (still timed from the first look)
  EXPECT_FALSE(arrival.saw(false, 0.3)); // (answered: gone)
  EXPECT_FALSE(arrival.moving(0.3));
  EXPECT_FLOAT_EQ(arrival.down(0.3), 1);
  EXPECT_TRUE(arrival.saw(true, 5)); // (the next offer)
  EXPECT_TRUE(arrival.moving(5));
}

// B, or Animations Off: in place at once
TEST(TradeArrivalTest, ASkipPutsTheScreenInPlace) {
  TradeArrival arrival;
  ASSERT_TRUE(arrival.saw(true, 0));
  arrival.skip();
  EXPECT_FALSE(arrival.moving(0));
  EXPECT_FLOAT_EQ(arrival.down(0), 1);
  EXPECT_FALSE(arrival.saw(true, 0.1)); // (and it doesn't come again)
  EXPECT_FLOAT_EQ(arrival.down(0.1), 1);
}

// Its banner waiting its turn (a show has the stage, or another banner's
// words go first): the board stays until the banner starts, and the screen
// comes down timed from then
TEST(TradeArrivalTest, ItWaitsForItsBanner) {
  TradeArrival arrival;
  ASSERT_TRUE(arrival.saw(true, 0));
  arrival.waitForBanner();
  EXPECT_FLOAT_EQ(arrival.down(30), 0);
  EXPECT_TRUE(arrival.moving(30));
  arrival.after(40);
  EXPECT_FLOAT_EQ(arrival.down(40 + kWordsSettled - 0.01), 0);
  const double inPlace = 40 + TradeArrival::kStartsAfter + TradeArrival::kSeconds + 0.01;
  EXPECT_FLOAT_EQ(arrival.down(inPlace), 1);
  EXPECT_FALSE(arrival.moving(inPlace));
  // (answered while it waited: gone, and nothing timed for the next look)
  EXPECT_FALSE(arrival.saw(false, 41));
  arrival.after(50);
  EXPECT_FALSE(arrival.moving(50));
}

// Animations Quick: on the banner's clock at twice the pace, in place in
// half the real time
TEST(TradeArrivalTest, QuickBringsItInTwiceAsFast) {
  using Clock = ScratchPadd::GameClock::RealClock;
  const Clock::time_point start{};
  ScratchPadd::GameClock clock(start);
  clock.setRate(rateOf(Animations::Quick), start);
  TradeArrival arrival;
  ASSERT_TRUE(arrival.saw(true, clock.seconds(start)));
  const double realSeconds = (TradeArrival::kStartsAfter + TradeArrival::kSeconds) / 2;
  const auto inPlace = start + std::chrono::duration_cast<Clock::duration>(
                                   std::chrono::duration<double>(realSeconds + 0.001));
  EXPECT_FALSE(arrival.moving(clock.seconds(inPlace)));
  EXPECT_FLOAT_EQ(arrival.down(clock.seconds(inPlace)), 1);
}

// The banner's words fit across each handheld's top screen in its font (the
// title at the game's banner size): the longest bot's name in each
// headline (TradeRoom::headline), and a bot's largest offer (two of a kind
// for one). (A counter's once said "COUNTERS YOUR OFFER": too wide for a DS)
TEST(TradeArrivalTest, TheBannersWordsFitAcross) {
  namespace Scene = ScratchPadd::Scene;
  constexpr float kTitleScale = 2; // (ScreenPadd::drawBand)
  const std::string note = "2 Crystals for your 1 Minerals";
  for (const auto &[font, width] : {std::pair{&Scene::Font::silkscreen(), 256.0f},
                                    std::pair{&Scene::Font::orbitron(), 400.0f}}) {
    for (const std::string title : {"SIRIUS OFFERS A TRADE", "SIRIUS COUNTERS"})
      EXPECT_LE(font->width(title, kTitleScale), width) << title;
    EXPECT_LE(font->width(note), width);
  }
}
