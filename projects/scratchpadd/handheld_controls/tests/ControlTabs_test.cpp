// ControlTabs: every padd's controls as tabs on a text screen, without a
// handheld: tabs from snapshots, choosing and stepping controls, the changes
// sent to their owners, and the lines it lays out.
#include <ScratchPadd/HandheldControls/ControlTabs.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace ScratchPadd;
using namespace ScratchPadd::HandheldControls;

namespace {

MessageType::ControlSnapshot snapshotOf(const std::string &padd, ControlValueMap controls) {
  return {.paddName = padd, .controlMap = std::move(controls)};
}

// A panel the DS's size, recording the changes it sends
struct Panel {
  std::vector<std::pair<std::string, ControlTypeVariant>> sent;
  ControlTabs tabs{[this](PaddAddress, const MessageType::ControlChange &change) {
                     sent.emplace_back(change.controlName, change.controlTypeValue);
                   },
                   32, 24};

  Panel() {
    tabs.show(snapshotOf("AudioPadd", {{"master", ControlType::Double(1.0, {{0.0, 1.0}})},
                                       {"mute", ControlType::Boolean(false)},
                                       {"music", ControlType::Double(0.5, {{0.0, 1.0}})}}),
              PaddAddress{});
    tabs.show(
        snapshotOf("RagdollPadd", {{"Sub-steps", ControlType::Integer(2, {{1, 4}})},
                                   {"Steps a second", ControlType::String("30", {{"30", "60"}})},
                                   {"Name", ControlType::String("free text")}}),
        PaddAddress{});
  }

  // The text of row `row`, its segments joined
  std::string text(int row) {
    std::string joined;
    const std::vector<Line> lines = tabs.lines(); // kept: the loop reads one of them
    for (const Segment &segment : lines[static_cast<size_t>(row)])
      joined += segment.text;
    return joined;
  }
};

Input press(Input input) { return input; }

} // namespace

// A tab for each padd with controls, in the order they answered; a padd's
// new snapshot updates its tab; an empty one removes it.
TEST(ControlTabsTest, ATabForEachPaddWithControls) {
  Panel panel;
  EXPECT_EQ(panel.tabs.tabNames(), (std::vector<std::string>{"AudioPadd", "RagdollPadd"}));
  panel.tabs.show(snapshotOf("AudioPadd", {{"master", ControlType::Double(0.2)}}), PaddAddress{});
  EXPECT_EQ(panel.tabs.tabNames().size(), 2u);
  panel.tabs.handle(press({.r = true}));
  EXPECT_EQ(panel.tabs.tab(), 1u);
  panel.tabs.show(snapshotOf("RagdollPadd", {}), PaddAddress{});
  EXPECT_EQ(panel.tabs.tabNames(), std::vector<std::string>{"AudioPadd"});
  EXPECT_EQ(panel.tabs.tab(), 0u); // back on the one that's left
}

// L and R change tabs (stopping at the ends); up and down choose a control,
// remembered for each tab.
TEST(ControlTabsTest, ButtonsChooseTabsAndControls) {
  Panel panel;
  panel.tabs.handle(press({.l = true}));
  EXPECT_EQ(panel.tabs.tab(), 0u);
  panel.tabs.handle(press({.down = true}));
  panel.tabs.handle(press({.down = true}));
  EXPECT_EQ(panel.tabs.row(), 2u);
  panel.tabs.handle(press({.down = true})); // the last
  EXPECT_EQ(panel.tabs.row(), 2u);
  panel.tabs.handle(press({.r = true}));
  EXPECT_EQ(panel.tabs.tab(), 1u);
  EXPECT_EQ(panel.tabs.row(), 0u);
  panel.tabs.handle(press({.r = true}));
  EXPECT_EQ(panel.tabs.tab(), 1u);
  panel.tabs.handle(press({.l = true}));
  EXPECT_EQ(panel.tabs.row(), 2u);
}

// Left and right step a number through its range (a twentieth at a time,
// stopping at the ends), an integer by one, a string through its allowed
// values round and round, and set a boolean; A toggles a boolean. Each
// change is sent to the control's padd and shown at once. Free text isn't
// edited.
TEST(ControlTabsTest, StepsValuesAndSendsTheChanges) {
  Panel panel;
  panel.tabs.handle(press({.right = true})); // master, already at the top of its range
  EXPECT_TRUE(panel.sent.empty());
  panel.tabs.handle(press({.left = true}));
  ASSERT_EQ(panel.sent.size(), 1u);
  EXPECT_EQ(panel.sent[0].first, "master");
  EXPECT_DOUBLE_EQ(std::get<ControlType::Double>(panel.sent[0].second).value, 0.95);
  EXPECT_NE(panel.text(2).find("0.95"), std::string::npos);

  panel.tabs.handle(press({.down = true})); // music (sorted: master, music, mute)
  panel.tabs.handle(press({.down = true})); // mute
  panel.tabs.handle(press({.a = true}));
  EXPECT_TRUE(std::get<ControlType::Boolean>(panel.sent.back().second).value);
  panel.tabs.handle(press({.left = true}));
  EXPECT_FALSE(std::get<ControlType::Boolean>(panel.sent.back().second).value);

  panel.tabs.handle(press({.r = true})); // RagdollPadd: Name, Steps a second, Sub-steps
  const size_t before = panel.sent.size();
  panel.tabs.handle(press({.right = true})); // Name: free text
  EXPECT_EQ(panel.sent.size(), before);
  panel.tabs.handle(press({.down = true}));
  panel.tabs.handle(press({.right = true}));
  EXPECT_EQ(std::get<ControlType::String>(panel.sent.back().second).value, "60");
  panel.tabs.handle(press({.right = true}));
  EXPECT_EQ(std::get<ControlType::String>(panel.sent.back().second).value, "30"); // round again
  panel.tabs.handle(press({.down = true}));
  panel.tabs.handle(press({.left = true}));
  EXPECT_EQ(std::get<ControlType::Integer>(panel.sent.back().second).value, 1);
  panel.tabs.handle(press({.left = true})); // the bottom of its range
  EXPECT_EQ(std::get<ControlType::Integer>(panel.sent.back().second).value, 1);
}

// The tabs across the top (the chosen one in brackets and highlighted,
// "Padd" left off), a rule, the controls with their values at the right
// (the chosen one marked), and the help at the bottom, each within the
// screen's width.
TEST(ControlTabsTest, LaysOutTabsControlsAndHelp) {
  Panel panel;
  const std::vector<Line> lines = panel.tabs.lines();
  ASSERT_EQ(lines.size(), 24u);
  ASSERT_EQ(lines[0].size(), 2u);
  EXPECT_EQ(lines[0][0].text, "[Audio]");
  EXPECT_TRUE(lines[0][0].highlighted);
  EXPECT_EQ(lines[0][1].text, " Ragdoll ");
  EXPECT_FALSE(lines[0][1].highlighted);
  EXPECT_EQ(panel.text(1), std::string(32, '-'));
  EXPECT_EQ(panel.text(2).substr(0, 8), "> master");
  EXPECT_EQ(panel.text(2).substr(28), "1.00");
  EXPECT_EQ(panel.text(3).substr(0, 7), "  music");
  EXPECT_EQ(panel.text(3).substr(28), "0.50");
  EXPECT_EQ(panel.text(4).substr(0, 6), "  mute");
  EXPECT_EQ(panel.text(4).substr(29), "[ ]");
  for (int row = 0; row < 24; ++row)
    EXPECT_LE(panel.text(row).size(), 32u) << "row " << row;
  EXPECT_NE(panel.text(23).find("L/R"), std::string::npos);
}

// Tapping a tab chooses it; tapping a control's row chooses that control.
TEST(ControlTabsTest, TouchChoosesTabsAndControls) {
  Panel panel;
  panel.tabs.handle(press({.touched = {{10, 0}}})); // " Ragdoll " starts at column 7
  EXPECT_EQ(panel.tabs.tab(), 1u);
  panel.tabs.handle(press({.touched = {{3, 4}}})); // the third control's row
  EXPECT_EQ(panel.tabs.row(), 2u);
  panel.tabs.handle(press({.touched = {{3, 20}}})); // under the list: nothing
  EXPECT_EQ(panel.tabs.row(), 2u);
}

// SELECT hides it and shows it again; hidden, it ignores the other buttons.
// It's marked for redrawing only when something shown changes.
TEST(ControlTabsTest, SelectHidesItAndOnlyChangesRedraw) {
  Panel panel;
  EXPECT_TRUE(panel.tabs.dirty());
  panel.tabs.lines();
  EXPECT_FALSE(panel.tabs.handle(press({.up = true}))); // already at the top
  EXPECT_FALSE(panel.tabs.dirty());
  panel.tabs.handle(press({.select = true}));
  EXPECT_FALSE(panel.tabs.visible());
  panel.tabs.handle(press({.down = true}));
  EXPECT_EQ(panel.tabs.row(), 0u);
  panel.tabs.handle(press({.select = true}));
  EXPECT_TRUE(panel.tabs.visible());
  EXPECT_TRUE(panel.tabs.dirty());
}

// With more controls than rows, the list scrolls to keep the chosen one on
// screen; with more tabs than fit, the row of tabs scrolls to the chosen one.
TEST(ControlTabsTest, ScrollsToKeepTheChosenOnScreen) {
  ControlTabs tabs({}, 32, 8); // 5 rows for controls
  ControlValueMap many;
  for (char c = 'a'; c <= 'j'; ++c)
    many.emplace(std::string("control ") + c, ControlType::Integer(0));
  tabs.show(snapshotOf("First", many), PaddAddress{});
  for (int i = 0; i < 7; ++i)
    tabs.handle({.down = true});
  const std::vector<Line> lines = tabs.lines();
  EXPECT_EQ(lines[6][0].text.substr(0, 11), "> control h"); // the last of the list's rows

  for (const char *padd : {"SecondPaddWithALongName", "ThirdPaddWithALongName"})
    tabs.show(snapshotOf(padd, {{"x", ControlType::Integer(0)}}), PaddAddress{});
  tabs.handle({.r = true});
  tabs.handle({.r = true});
  const Line tabRow = tabs.lines()[0];
  ASSERT_FALSE(tabRow.empty());
  EXPECT_TRUE(tabRow.back().highlighted);
  EXPECT_EQ(tabRow.back().text, "[ThirdPaddWithALongName]");
}

TEST(ControlTabsTest, DescribesValues) {
  EXPECT_EQ(describe(ControlType::Double(0.25)), "0.25");
  EXPECT_EQ(describe(ControlType::Integer(7)), "7");
  EXPECT_EQ(describe(ControlType::Boolean(true)), "[x]");
  EXPECT_EQ(describe(ControlType::String("abc")), "abc");
}
