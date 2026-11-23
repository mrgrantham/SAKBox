// The choosing screens drawn (ChoiceScreens): a touch on each part finds
// it, on a DS's screen and a 3DS's; the build screen offers Build only for
// a row that can be built; the pickers' cards draw, with their titles; and
// each screen is written out as a picture to look at
#include <CosmicHex/Handheld/CardFace.hpp>
#include <CosmicHex/Handheld/ChoiceScreens.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <ScratchPadd/Scene/Font.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using What = ChoiceScreens::Touch::What;

namespace {
void writeOut(const Canvas &canvas, const std::string &name) {
  const char *dir = std::getenv("TEST_UNDECLARED_OUTPUTS_DIR");
  if (!dir)
    return;
  std::ofstream out(std::string(dir) + "/" + name, std::ios::binary);
  out << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
  for (const auto &c : canvas.pixels())
    out.put(static_cast<char>(c.r)).put(static_cast<char>(c.g)).put(static_cast<char>(c.b));
}

State atFirstMain() {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator random(11);
  for (int guard = 0; guard < 2000 && state.phase != Phase::Main; ++guard)
    Game::apply(state, Game::currentPlayer(state) == GameBots::kDealer
                           ? Game::dealerAction(state, random)
                           : Game::legalActions(state).front());
  return state;
}
} // namespace

TEST(ChoiceScreensTest, TheBuildScreen) {
  for (const auto [w, h] : {std::pair{256, 192}, std::pair{320, 240}}) {
    State state = atFirstMain();
    state.player[static_cast<std::size_t>(state.turn)].resources = {2, 2, 1, 1, 1};
    const auto rows = buildRows(state);
    Canvas canvas(w, h);
    // A row that can be built, chosen: Build shows; touches find each part
    int canBuild = -1;
    for (std::size_t i = 0; i < rows.size(); ++i)
      if (rows[i].canBuild && canBuild < 0)
        canBuild = static_cast<int>(i);
    ASSERT_GE(canBuild, 0);
    ChoiceScreens::drawBuild(canvas, rows, canBuild);
    writeOut(canvas, "build_" + std::to_string(w) + ".ppm");
    const float k = static_cast<float>(w) / 256;
    const auto row1 =
        ChoiceScreens::buildTouch(canvas.width(), rows.size(), w / 2, static_cast<int>(50 * k));
    ASSERT_TRUE(row1.has_value());
    if (row1) {
      EXPECT_EQ(row1->what, What::Item);
      EXPECT_EQ(row1->item, 1);
    }
    const auto back = ChoiceScreens::buildTouch(
        canvas.width(), rows.size(), static_cast<int>(10 * k), static_cast<int>(178 * k));
    ASSERT_TRUE(back.has_value());
    if (back) {
      EXPECT_EQ(back->what, What::Back);
    }
    // Build's button green where it is; with nothing buildable chosen, not
    const auto green = canvas.at(static_cast<int>(200 * k), static_cast<int>(171 * k));
    EXPECT_GT(green.g, green.r);
    Canvas none(w, h);
    ChoiceScreens::drawBuild(none, rows, -1);
    EXPECT_EQ(none.at(static_cast<int>(200 * k), static_cast<int>(171 * k)).g, 24);
  }
}

TEST(ChoiceScreensTest, TheResourcePicker) {
  std::vector<Action> legal;
  legal.reserve(kResources);
  for (int r = 0; r < kResources; ++r)
    legal.push_back({Action::Kind::ChooseHome, r});
  const Pickers::ResourceChoice choice{"Your home pays 1", "Choose which resource", 1, legal};
  for (const int w : {256, 320}) {
    Canvas canvas(w, w * 3 / 4);
    CardSprites cards(pickerCardWidth(w));
    while (!cards.prepare()) {
    }
    ChoiceScreens::drawResources(canvas, choice, {Resource::Fuel}, {4, 0, 1, 1, 1}, false, cards);
    writeOut(canvas, "resources_" + std::to_string(w) + ".ppm");
    const float k = static_cast<float>(w) / 256;
    for (int r = 0; r < kResources; ++r) {
      const auto touched = ChoiceScreens::resourcesTouch(
          canvas.width(), static_cast<int>((6 + 49 * r + 24) * k), static_cast<int>(60 * k));
      ASSERT_TRUE(touched.has_value());
      if (touched) {
        EXPECT_EQ(touched->item, r);
      }
    }
    // Each card drawn: its band's colour where its title goes, not the ground
    const auto band = canvas.at(static_cast<int>((6 + 49 * 2 + 4) * k), static_cast<int>(45 * k));
    EXPECT_NE(band.b, 42);
  }
}

TEST(ChoiceScreensTest, ThePlayerPicker) {
  State state = newGame(4);
  Pickers::PlayerChoice choice{
      3, {{1, 5, {Action::Kind::MoveBlockade, 3, 1}}, {3, 9, {Action::Kind::MoveBlockade, 3, 3}}}};
  Canvas canvas(256, 192);
  CardSprites cards(pickerCardWidth(256));
  ChoiceScreens::drawPlayers(canvas, choice, 0, 2, Art::CardBack::Rings, cards);
  writeOut(canvas, "players_256.ppm");
  const auto first = ChoiceScreens::playersTouch(canvas.width(), 2, 128, 60);
  const auto second = ChoiceScreens::playersTouch(canvas.width(), 2, 128, 120);
  ASSERT_TRUE(first.has_value() && second.has_value());
  if (first && second) {
    EXPECT_EQ(first->item, 0);
    EXPECT_EQ(second->item, 1);
  }
  // Which card: each 9 pixels on from the last, from 80 across
  const auto third = ChoiceScreens::playersTouch(canvas.width(), 2, 80 + 2 * 9 + 3, 60);
  ASSERT_TRUE(third.has_value());
  if (third) {
    EXPECT_EQ(third->card, 2);
  }
}

// The cards screen: four cards, the chosen one's words under them and Play
// (a touch on each card finds it; Back and Play at the foot); a Charter not
// met yet says so; and the keep screen's two cards
TEST(ChoiceScreensTest, TheCardsScreens) {
  const std::vector<HeldCard> held{{.contract = 99,
                                    .card = Art::Card::Enforcer,
                                    .kind = Action::Kind::PlayEnforcer,
                                    .playable = true},
                                   {.contract = 1,
                                    .card = Art::Card::SupplyDrop,
                                    .kind = Action::Kind::PlaySupplyDrop,
                                    .playable = true},
                                   {.contract = 2,
                                    .card = Art::Card::CharterOccupier,
                                    .charter = true,
                                    .kind = Action::Kind::RevealContract,
                                    .playable = false,
                                    .why = "Not met yet"},
                                   {.contract = 3,
                                    .card = Art::Card::ShipyardSurplus,
                                    .kind = Action::Kind::PlayShipyard,
                                    .playable = true}};
  for (const int w : {256, 320}) {
    Canvas canvas(w, w * 3 / 4);
    CardSprites sprites(1); // (resized to the screen's cards as it's drawn)
    ChoiceScreens::drawCards(canvas, held, 0, sprites);
    writeOut(canvas, "cards_" + std::to_string(w) + ".ppm");
    ChoiceScreens::drawCards(canvas, held, 2, sprites);
    writeOut(canvas, "cards_charter_" + std::to_string(w) + ".ppm");
    // Each card is found where it's drawn, in turn
    int found = 0;
    for (int y = 0; y < w * 3 / 4; y += 2)
      for (int x = 0; x < w; x += 2)
        if (const auto t = ChoiceScreens::cardsTouch(w, w * 3 / 4, held.size(), x, y);
            t && t->what == What::Item && t->item == found)
          ++found;
    EXPECT_EQ(found, 4);
    const KeepChoice keep{
        .cards = {Art::Card::InterstellarCartel, Art::Card::CharterDeepSpaceRelay},
        .actions = {Action{Action::Kind::KeepContract, 5}, Action{Action::Kind::KeepContract, 6}}};
    ChoiceScreens::drawKeep(canvas, keep, 1, sprites);
    writeOut(canvas, "keep_" + std::to_string(w) + ".ppm");
  }
}

// The trade room drawn: the bank's tab with a trade picked, and answering
// an offer (pictures in the test's outputs)
TEST(ChoiceScreensTest, TheTradeRoom) {
  State state = newGame(4, agreedRules(boardR3(), 4), 0);
  state.player[0].resources = {5, 1, 2, 0, 1};
  state.phase = Phase::Trading;
  TradeRoom room;
  room.update(state, Game::legalActions(state), true);
  room.add(TradeRoom::Side::Give, Resource::Minerals);
  room.add(TradeRoom::Side::Get, Resource::Food);
  for (const int w : {256, 320}) {
    const int h = w * 3 / 4;
    Canvas canvas(w, h);
    CardSprites cards(1); // (resized to the trade room's cards as it's drawn)
    TradeRoom shown = room;
    ChoiceScreens::drawTrade(canvas, shown, cards);
    writeOut(canvas, "trade_bank_" + std::to_string(w) + ".ppm");
    EXPECT_TRUE(ChoiceScreens::tradeTouch(w, h, shown, w / 12, h / 20).has_value()); // (a tab)
    shown.choosePartner(2);
    shown.add(TradeRoom::Side::Give, Resource::Fuel);
    shown.add(TradeRoom::Side::Give, Resource::Fuel);
    shown.add(TradeRoom::Side::Get, Resource::Water);
    ChoiceScreens::drawTrade(canvas, shown, cards);
    writeOut(canvas, "trade_player_" + std::to_string(w) + ".ppm");
    // Each card and its minus found where they're drawn, in both rows
    const ChoiceScreens::TradeLayout l = ChoiceScreens::tradeLayout(w, h);
    using TradeWhat = ChoiceScreens::TradeTouch::What;
    const auto middle = [](const ChoiceScreens::Box &b) {
      return std::pair{static_cast<int>(b.x + b.w / 2), static_cast<int>(b.y + b.h / 2)};
    };
    for (const auto &[side, top] :
         {std::pair{TradeRoom::Side::Give, l.giveTop}, std::pair{TradeRoom::Side::Get, l.getTop}})
      for (int r = 0; r < kResources; ++r) {
        const auto [cx, cy] = middle(l.card(top, r));
        const auto [mx, my] = middle(l.minus(top, r));
        const auto card = ChoiceScreens::tradeTouch(w, h, shown, cx, cy);
        const auto minus = ChoiceScreens::tradeTouch(w, h, shown, mx, my);
        ASSERT_TRUE(card.has_value() && minus.has_value());
        if (card && minus) {
          EXPECT_EQ(card->what, TradeWhat::Add);
          EXPECT_EQ(card->side, side);
          EXPECT_EQ(card->resource, static_cast<Resource>(r));
          EXPECT_EQ(minus->what, TradeWhat::Remove);
          EXPECT_EQ(minus->side, side);
          EXPECT_EQ(minus->resource, static_cast<Resource>(r));
        }
      }
  }
}

namespace {
namespace Scene = ScratchPadd::Scene;
float silkscreenLine(float scale) { return Scene::Font::silkscreen().lineHeight(scale); }
float silkscreenWidth(std::string_view text, float scale) {
  return Scene::Font::silkscreen().width(text, scale);
}
float orbitronLine(float scale) { return Scene::Font::orbitron().lineHeight(scale); }
float orbitronWidth(std::string_view text, float scale) {
  return Scene::Font::orbitron().width(text, scale);
}
// Each console's bottom screen in its own font: the DS's in Silkscreen,
// the 3DS's in Orbitron
struct Screen {
  int width, height;
  TextMeasure measure;
};
const std::array<Screen, 2> kScreens{Screen{256, 192, {silkscreenLine, silkscreenWidth}},
                                     Screen{320, 240, {orbitronLine, orbitronWidth}}};
bool inside(const ChoiceScreens::Box &inner, const ChoiceScreens::Box &outer) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
         inner.y + inner.h <= outer.y + outer.h;
}
bool apart(const ChoiceScreens::Box &a, const ChoiceScreens::Box &b) {
  return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
}
// The buttons' row's top (the screens' Back, a DS's 168 scaled)
float buttonsTop(int width) { return 168.0f * static_cast<float>(width) / 256; }
} // namespace

// The trade room shows whole cards, 2:3 (CardFace's one shape), on both
// consoles' screens in their fonts: each row of five inside the screen,
// under its label, the two rows apart, the deal under them and over the
// buttons; on each card its count on its title band, and its minus and
// how many you hold ("of 9" fitting) in its foot, clear of its icon
TEST(ChoiceScreensTest, TheTradeRoomShowsWholeCards) {
  for (const Screen &s : kScreens) {
    const ChoiceScreens::TradeLayout l = ChoiceScreens::tradeLayout(s.width, s.height, s.measure);
    SCOPED_TRACE(s.width);
    std::printf("trade room on %d x %d: cards %d x %d\n", s.width, s.height, l.cardWidth,
                l.cardHeight);
    EXPECT_EQ(l.cardHeight, CardFace::heightOf(l.cardWidth));
    EXPECT_GE(l.cardWidth, 32 * s.width / 256); // (as big as they were)
    const ChoiceScreens::Box screen{0, 0, static_cast<float>(s.width), buttonsTop(s.width)};
    EXPECT_GE(l.getLabel.top, 20.0f * static_cast<float>(s.width) / 256); // (under the tabs)
    EXPECT_LE(l.getLabel.bottom, l.getTop);
    EXPECT_LE(l.getTop + static_cast<float>(l.cardHeight), l.giveLabel.top);
    EXPECT_LE(l.giveLabel.bottom, l.giveTop);
    EXPECT_LE(l.giveTop + static_cast<float>(l.cardHeight), l.deal.top);
    EXPECT_LE(l.deal.bottom, buttonsTop(s.width));
    for (const float top : {l.getTop, l.giveTop})
      for (int r = 0; r < kResources; ++r) {
        const ChoiceScreens::Box card = l.card(top, r);
        EXPECT_TRUE(inside(card, screen)) << r;
        if (r > 0) {
          EXPECT_TRUE(apart(card, l.card(top, r - 1))) << r;
        }
        EXPECT_TRUE(inside(l.count(top, r), card)) << r;
        EXPECT_TRUE(inside(l.minus(top, r), card)) << r;
        EXPECT_TRUE(apart(l.minus(top, r), l.held(top, r))) << r;
        EXPECT_LE(s.measure.width("of 9", 1), l.held(top, r).w) << r;
        // (the foot under the icon)
        const Art::CardArea icon = CardFace::iconOf(static_cast<Art::Card>(r));
        const float u = card.w / Art::kCardWidth;
        EXPECT_GE(l.minus(top, r).y, card.y + (icon.y + icon.height) * u) << r;
        EXPECT_LE(l.count(top, r).y + l.count(top, r).h, card.y + icon.y * u) << r;
      }
  }
}

// The cards screen shows whole cards, 2:3, for one card to the most it
// shows, on both consoles' screens in their fonts: inside the screen,
// under its heading and over the chosen card's words, no two touching,
// as big as they were for four (a DS's 56, four across) and never less
// than half that; the words over the buttons
TEST(ChoiceScreensTest, TheCardsScreenShowsWholeCards) {
  for (const Screen &s : kScreens)
    for (std::size_t n = 1; n <= ChoiceScreens::CardsLayout::kMostCards; ++n) {
      SCOPED_TRACE(std::to_string(s.width) + " " + std::to_string(n));
      const ChoiceScreens::CardsLayout l =
          ChoiceScreens::cardsLayout(s.width, s.height, n, s.measure);
      ASSERT_EQ(l.cards.size(), n);
      std::printf("cards screen on %d x %d, %zu cards: %d x %d\n", s.width, s.height, n,
                  l.cardWidth, l.cardHeight);
      EXPECT_EQ(l.cardHeight, CardFace::heightOf(l.cardWidth));
      EXPECT_GE(l.cardWidth, 26 * s.width / 256); // (eight: two rows of four, near half of four's)
      if (n <= 4) {
        EXPECT_GE(l.cardWidth, 48 * s.width / 256); // (in one row, as big as before or near it)
        EXPECT_LE(l.cardWidth, 56 * s.width / 256);
      }
      const ChoiceScreens::Box room{0, l.heading.bottom, static_cast<float>(s.width),
                                    l.wordsTop - l.heading.bottom};
      for (std::size_t i = 0; i < n; ++i) {
        EXPECT_TRUE(inside(l.cards[i], room)) << i;
        EXPECT_EQ(static_cast<int>(l.cards[i].h),
                  CardFace::heightOf(static_cast<int>(l.cards[i].w)))
            << i;
        for (std::size_t j = 0; j < i; ++j)
          EXPECT_TRUE(apart(l.cards[i], l.cards[j])) << i << " " << j;
      }
      EXPECT_LE(l.wordsTop + Scene::stackHeight(
                                 std::array{1.0f, s.width >= 307 ? 1.5f : 1.0f, 1.0f, 1.0f, 1.0f},
                                 2.0f * static_cast<float>(s.width) / 256, s.measure.lineHeight),
                buttonsTop(s.width) + 0.01f);
    }
}
