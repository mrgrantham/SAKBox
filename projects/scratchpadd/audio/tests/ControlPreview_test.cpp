// What AudioPadd plays when its controls are changed from a panel, and how
// often (AudioPadd/ControlPreview.hpp)
#include <AudioPadd/ControlPreview.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

using namespace AudioPaddPreview;
using ScratchPadd::Audio::Group;
using ScratchPadd::ControlType::Boolean;
using ScratchPadd::ControlType::Double;

namespace {

// The preview for a change, or one with no keys if there's none
Preview previewOf(const std::string &name, const ScratchPadd::ControlTypeVariant &previous,
                  const ScratchPadd::ControlTypeVariant &current) {
  return forChange(name, previous, current).value_or(Preview{.group = Group::Sounds});
}

} // namespace

TEST(ControlPreviewTest, EachVolumeHasItsOwnPreview) {
  const Preview master = previewOf("master", Double(1.0), Double(0.5));
  EXPECT_EQ(master.group, Group::Sounds);
  EXPECT_EQ(master.keys.size(), 1u);

  const Preview sounds = previewOf("sounds", Double(1.0), Double(0.5));
  EXPECT_EQ(sounds.group, Group::Sounds);
  EXPECT_EQ(sounds.keys.size(), 1u);
  EXPECT_NE(sounds.keys, master.keys); // told apart by ear

  const Preview music = previewOf("music", Double(1.0), Double(0.5));
  EXPECT_EQ(music.group, Group::Music);
  EXPECT_EQ(music.keys.size(), 3u); // a chord
}

// Unmuting is heard; muting is the silence; other controls play nothing
TEST(ControlPreviewTest, OnlyUnmutingIsHeard) {
  const Preview unmuted = previewOf("mute", Boolean(true), Boolean(false));
  EXPECT_EQ(unmuted.group, Group::Sounds);
  EXPECT_EQ(unmuted.keys.size(), 3u);
  EXPECT_FALSE(forChange("mute", Boolean(false), Boolean(true)));
  EXPECT_FALSE(forChange("mute", Boolean(false), Boolean(false)));
  EXPECT_FALSE(forChange("previews", Boolean(true), Boolean(false)));
  EXPECT_FALSE(forChange("something else", Double(0), Double(1)));
}

// A swipe's many changes start a preview at most every kPreviewGap
TEST(ControlPreviewTest, PreviewsAreSpacedOut) {
  Pacer pacer;
  const Pacer::Clock::time_point start{};
  EXPECT_TRUE(pacer.start(start));
  EXPECT_FALSE(pacer.start(start + kPreviewGap / 2));
  EXPECT_FALSE(pacer.start(start + kPreviewGap - std::chrono::milliseconds(1)));
  EXPECT_TRUE(pacer.start(start + kPreviewGap));
  EXPECT_FALSE(pacer.start(start + kPreviewGap + std::chrono::milliseconds(1)));
}
