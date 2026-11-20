// A close-up's slot: its picture drawn when asked for, and only when it
// isn't there already; a picture of the wrong size refused
#include <ExpectedCapabilities/ExpectedCapabilities.hpp>
#include <ScratchPadd/PictureSlot/PictureSlot.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>

#include <gtest/gtest.h>

using namespace ScratchPadd;

TEST(PictureSlotTest, DrawsOnlyWhatIsntThereAlready) {
  Scene::Display display{64, 64, "PictureSlot_test", false};
  if (!display.isOpen())
    SKIP_OR_FAIL_WITHOUT("display", "no window to draw in: " + display.error());
  Scene::Renderer renderer(display);
  PictureSlot slot;
  slot.load(renderer, 8, 8);
  int drawn = 0;
  const auto red = [&] {
    ++drawn;
    return std::vector<Scene::Colour>(64, Scene::Colour{255, 0, 0});
  };
  EXPECT_TRUE(slot.show(renderer, 3, red));
  EXPECT_TRUE(slot.showing(3));
  EXPECT_TRUE(slot.show(renderer, 3, red)); // (there already: not drawn again)
  EXPECT_EQ(drawn, 1);
  EXPECT_TRUE(slot.show(renderer, 4, red));
  EXPECT_EQ(drawn, 2);
  EXPECT_FALSE(slot.showing(3));
  // A picture of the wrong size: refused, the slot showing nothing
  EXPECT_FALSE(slot.show(renderer, 5, [] { return std::vector<Scene::Colour>(10); }));
  EXPECT_FALSE(slot.showing(4));
  EXPECT_FALSE(slot.showing(5));
  // Never loaded: nothing
  PictureSlot none;
  EXPECT_FALSE(none.show(renderer, 1, red));
}
