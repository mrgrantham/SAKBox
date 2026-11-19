// The choosing screens drawn (ChoiceScreens): a touch on each part finds
// it, on a DS's screen and a 3DS's; the build screen offers Build only for
// a row that can be built; the pickers' cards draw, with their titles; and
// each screen is written out as a picture to look at
#include <CosmicHex/Handheld/ChoiceScreens.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>

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
  for (int r = 0; r < kResources; ++r)
    legal.push_back({Action::Kind::ChooseHome, r});
  const Pickers::ResourceChoice choice{"Your home pays 1", "Choose which resource", 1, legal};
  for (const int w : {256, 320}) {
    Canvas canvas(w, w * 3 / 4);
    CardSprites cards(w);
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
  CardSprites cards(256);
  ChoiceScreens::drawPlayers(canvas, choice, 0, Art::CardBack::Rings, cards);
  writeOut(canvas, "players_256.ppm");
  const auto first = ChoiceScreens::playersTouch(canvas.width(), 2, 128, 60);
  const auto second = ChoiceScreens::playersTouch(canvas.width(), 2, 128, 120);
  ASSERT_TRUE(first.has_value() && second.has_value());
  if (first && second) {
    EXPECT_EQ(first->item, 0);
    EXPECT_EQ(second->item, 1);
  }
}
