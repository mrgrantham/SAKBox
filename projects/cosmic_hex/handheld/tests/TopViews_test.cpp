// Every view over the board drawn through its whole timeline on a real
// renderer (the Mac's): none may crash or draw outside what it's given, at
// any moment from before it starts to after it ends
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/Hud.hpp>
#include <CosmicHex/Handheld/TopViews.hpp>

#include <ExpectedCapabilities/ExpectedCapabilities.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Primitives.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

namespace {

class TopViewsTest : public testing::Test {
protected:
  void SetUp() override {
    if (!display.isOpen())
      SKIP_OR_FAIL_WITHOUT("display", "no window to draw in: " + display.error());
    renderer.emplace(display);
    const auto square = Scene::MeshBuilder().quad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {});
    pictures.pictureSquare = pictures.backSquare = pictures.resourceSquare =
        pictures.closeUpCardSquare = renderer->load(square.data());
    pictures.hexTile = pictures.numberChip =
        renderer->load(Scene::MeshBuilder().box({}, {1, 0.1f, 1}).data());
    camera.eye = {0, 12, 9};
    camera.target = {0, 0, 0};
  }
  // `draw` in a frame at each of `times`
  void frames(const std::vector<double> &times, const std::function<void(const TopFrame &)> &draw) {
    for (const double now : times) {
      renderer->beginFrame(camera, {});
      const TopFrame frame{*renderer, renderer->overlay(), camera, 256, 192, pictures, names, now};
      draw(frame);
      renderer->endFrame();
    }
  }
  static std::vector<double> every(double from, double to, double step = 0.05) {
    std::vector<double> times;
    for (double t = from; t <= to; t += step)
      times.push_back(t);
    return times;
  }

  Scene::Display display{256, 192, "TopViews_test", false};
  std::optional<Scene::Renderer> renderer;
  TopPictures pictures;
  Scene::Camera camera;
  PlayerNames names = PlayerNames::forGame(4, 0, "James", 1);
  ScratchPadd::PictureSlot noCloseUp;
  State state = newGame(4, agreedRules(boardR3()));
};

} // namespace

// A home paying one, and two in a row (the die out once), for you and for
// a bot, the star's choice made at once
TEST_F(TopViewsTest, AHomePayFromStartToEnd) {
  pictures.homeDie = pictures.column = pictures.hexRing = pictures.hexTile;
  const std::array<ScratchPadd::PictureSlot, 2> slots{};
  for (const int face : {0, 3, kResources}) {
    for (const int player : {0, 2}) {
      for (const int pays : {1, 2}) {
        HomePayShow show;
        std::vector<TableGame::Played> rolls(static_cast<std::size_t>(pays),
                                             {player, Action{Action::Kind::RollHomeDie, face}});
        show.saw(rolls, state, 0);
        if (face == kResources)
          show.saw({{player, Action{Action::Kind::ChooseHome, 2}}}, state, 0);
        frames(every(-0.5, HomePayShow::seconds(pays) + 0.5), [&](const TopFrame &f) {
          show.update(f.now);
          drawHomePay(f, show, state, player == 0, slots);
        });
      }
    }
  }
}

TEST_F(TopViewsTest, ADiscoveryFromStartToEnd) {
  State after = state;
  for (Planet &planet : after.planets)
    if (!planet.flipped && planet.homeOf < 0) {
      planet.flipped = true;
      planet.kind = PlanetKind::IceMoon;
      planet.number = 5;
      break;
    }
  DiscoveryShow show;
  show.arrived(state, after, 0);
  frames(every(0, Discovery::kSeconds + 0.5), [&](const TopFrame &f) {
    show.update(f.now, nullptr);
    drawDiscovery(f, show, noCloseUp, noCloseUp);
  });
}

TEST_F(TopViewsTest, DealtCardsOtherPlayersAndTheFirstRoll) {
  DealtShow dealt;
  dealt.deal({Art::Card::Enforcer, Art::Card::CharterOccupier}, 0);
  dealt.closeUpDrawn(0);
  const std::array<ScratchPadd::PictureSlot, 2> slots{};
  frames(every(0, 3), [&](const TopFrame &f) { drawDealtCards(f, dealt, 1, slots); });
  frames(every(0, 1, 0.1), [&](const TopFrame &f) {
    drawOtherPlayer(f, viewOf(state, 2), static_cast<float>(f.now));
  });
  FirstRoll roll;
  roll.first = 1;
  roll.rounds = {{4, 6, 6, 2}, {0, 5, 3, 0}};
  FirstRollShow show;
  show.start(roll, 3, 1.0 / 30, 0);
  frames(every(0, 12, 0.1), [&](const TopFrame &f) {
    show.update(f.now);
    drawFirstRoll(f, roll, show);
  });
}

// Who goes first with two tied below the highest (the user's: two on a
// 3, nobody throwing again, looked like a bug): only the highest counts,
// so nobody throws again; once all have landed the others' numbers are
// dimmed and the screen says the rest follow round the table
TEST_F(TopViewsTest, AFirstRollWithATieBelowTheHighest) {
  FirstRoll roll;
  roll.first = 1;
  roll.rounds = {{3, 6, 3, 2}};
  FirstRollShow show;
  show.start(roll, 4, 1.0 / 30, 0);
  bool saidSo = false, landed = false;
  frames(every(0, 20, 0.1), [&](const TopFrame &f) {
    show.update(f.now);
    if (!show.allLanded())
      show.hurry(f.now); // (A: each die thrown at once)
    drawFirstRoll(f, roll, show);
    if (!show.allLanded() || show.sorted(f.now) <= 0)
      return;
    landed = true;
    for (const Scene::Overlay::Text &text : f.overlay.texts()) {
      saidSo = saidSo || text.text == "the rest follow round the table";
      if (text.text == "3" || text.text == "2") {
        EXPECT_EQ(text.colour, Palette::kDimWords) << text.text;
      }
      if (text.text == "6") {
        EXPECT_EQ(text.colour, Palette::kHighlight);
      }
    }
  });
  EXPECT_TRUE(landed);
  EXPECT_TRUE(saidSo);
  EXPECT_EQ(show.round(), 0); // (no second round)
}

TEST_F(TopViewsTest, ARollsPayFromStartToEnd) {
  for (Planet &planet : state.planets)
    if (!planet.flipped && planet.homeOf < 0) {
      planet.flipped = true;
      planet.kind = PlanetKind::LushPlanet;
      planet.number = 8;
      state.stations.push_back({.at = {planet.hex, 0}, .owner = 0, .colony = true});
      state.stations.push_back({.at = {planet.hex, 3}, .owner = 2});
      break;
    }
  RollPayShow show;
  show.saw({{1, Action{Action::Kind::Roll, 4, 4}}}, state);
  show.start(0);
  ASSERT_EQ(show.cards().size(), 3u); // (a colony's two, a station's one)
  frames(every(-0.2, show.seconds() + 0.3), [&](const TopFrame &f) {
    show.update(f.now);
    drawRollPay(f, show, state, 0);
  });
}

TEST_F(TopViewsTest, TheStartingHandDealtFromStartToEnd) {
  Settings settings = agreedRules(boardR3());
  settings.startCards = 3;
  const State dealing = newGame(4, settings, 1);
  constexpr int kCards = 4 * 3;
  std::vector<TableGame::Played> deal;
  deal.reserve(kCards);
  for (int i = 0; i < kCards; ++i)
    deal.push_back({-1, Action{Action::Kind::DealStartCard, (i * 2) % kResources}});
  for (const int you : {0, -1}) { // (playing, and watching)
    RollPayShow show;
    show.saw(deal, dealing);
    show.start(0);
    frames(every(-0.2, show.seconds() + 0.3), [&](const TopFrame &f) {
      show.update(f.now);
      drawRollPay(f, show, dealing, you);
    });
  }
}

TEST_F(TopViewsTest, ACardPlayedFromStartToEnd) {
  for (const int player : {0, 2}) {
    CardPlayShow show;
    show.saw({{player, Action{Action::Kind::PlaySupplyDrop, 0, 1}}}, 0);
    frames(every(-0.2, CardPlayShow::kSeconds + 0.2), [&](const TopFrame &f) {
      show.update(f.now);
      drawCardPlay(f, show, state, 0);
    });
  }
}

TEST_F(TopViewsTest, APickFromStartToEnd) {
  PickShow pick;
  pick.show(Resource::Food, 0);
  frames(every(0, 1, 0.05), [&](const TopFrame &f) {
    if (f.now > 0.5)
      pick.show(Resource::Crystals, f.now);
    drawPick(f, pick, "Your home pays 1", state.player[0].resources, state.settings, {255, 80, 80});
  });
}

TEST_F(TopViewsTest, CardsGainedFromStartToEnd) {
  HandShow hand;
  Resources added{};
  added[0] = 1;
  added[3] = 2;
  hand.gained(added);
  hand.start(0);
  Resources held{2, 0, 1, 4, 0};
  frames(every(-0.1, 2.5, 0.05), [&](const TopFrame &f) { drawGained(f, held, hand); });
}

// The HUD: the bars, the outline between rows, points popping, the corners,
// the last move and who won
TEST_F(TopViewsTest, TheHudDraws) {
  ScorePops pops;
  pops.saw({0, 0, 0, 0}, {0, 0, 0, 0}, 0);
  pops.saw({3, 1, 0, 2}, {1, 0, 0, 0}, 0);
  State won = state;
  won.winner = 2;
  frames(every(0, 1, 0.1), [&](const TopFrame &f) {
    drawHud(f, f.now < 0.5 ? state : won,
            {.outlineRow = static_cast<float>(f.now) * 3,
             .badgesOf = 0,
             .pops = &pops,
             .lastMove = "Lyra builds a station.",
             .winner = "Vega wins!"});
  });
}
