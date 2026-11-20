// Every view over the board drawn through its whole timeline on a real
// renderer (the Mac's): none may crash or draw outside what it's given, at
// any moment from before it starts to after it ends
#include <CosmicHex/Handheld/BoardPieces.hpp>
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

TEST_F(TopViewsTest, AHomePayFromStartToEnd) {
  for (const int face : {0, 3, kResources}) {
    for (const int player : {0, 2}) {
      HomePayShow show;
      show.saw({{player, Action{Action::Kind::RollHomeDie, face}}}, 0);
      if (face == kResources)
        show.saw({{player, Action{Action::Kind::ChooseHome, 2}}}, 0);
      frames(every(-0.5, HomePayShow::kSeconds + 0.5), [&](const TopFrame &f) {
        show.update(f.now);
        drawHomePay(f, show, state, player == 0, noCloseUp);
      });
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
    drawDiscovery(f, show, noCloseUp);
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
  frames(every(0, FirstRollShow::duration(roll)),
         [&](const TopFrame &f) { drawFirstRoll(f, roll, static_cast<float>(f.now)); });
}
