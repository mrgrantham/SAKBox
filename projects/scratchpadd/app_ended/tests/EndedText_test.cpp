#include <ScratchPadd/AppEnded/EndedText.hpp>
#include <gtest/gtest.h>

using ScratchPadd::AppEnded::endedText;
using ScratchPadd::AppEnded::Leaving;
using ScratchPadd::AppEnded::wrapLines;
using Lines = std::vector<std::string>;

TEST(EndedTextTest, WrapsAtSpacesAndNewLines) {
  EXPECT_EQ(wrapLines("Cyclone ragdoll", 31), Lines{"Cyclone ragdoll"});
  EXPECT_EQ(wrapLines("ScratchPadd audio self-test", 12),
            (Lines{"ScratchPadd", "audio", "self-test"}));
  EXPECT_EQ(wrapLines("one\ntwo three", 31), (Lines{"one", "two three"}));
  EXPECT_EQ(wrapLines("abcdefghij", 4), Lines{"abcd"}); // a word too wide is cut
  EXPECT_EQ(wrapLines("", 31), Lines{""});
}

TEST(EndedTextTest, TitleThenHasEndedThenWhatAButtonDoes) {
  const auto text = endedText("Cyclone ragdoll", "", Leaving::SwitchOff, 31);
  EXPECT_EQ(text.top, (Lines{"Cyclone ragdoll", "", "has ended."}));
  EXPECT_EQ(text.bottom, (Lines{"Press any button", "to switch off."}));
  EXPECT_EQ(endedText("X", "", Leaving::BackToMenu, 31).bottom,
            (Lines{"Press any button", "to go back to the menu."}));
}

TEST(EndedTextTest, AnAppsOwnMessage) {
  const auto text = endedText("Benchmark", "Results saved to the SD card.", Leaving::SwitchOff, 16);
  EXPECT_EQ(text.top, (Lines{"Benchmark", "", "Results saved to", "the SD card."}));
}

TEST(EndedTextTest, NothingWiderThanTheScreen) {
  const auto text = endedText("A very long title for a very small screen indeed",
                              "and a long message to go with it", Leaving::BackToMenu, 20);
  for (const auto *lines : {&text.top, &text.bottom})
    for (const auto &line : *lines)
      EXPECT_LE(line.size(), 20u) << line;
}
