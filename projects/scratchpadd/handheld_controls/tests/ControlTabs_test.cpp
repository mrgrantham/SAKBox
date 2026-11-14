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

PanelInput press(PanelInput input) { return input; }

// The stylus put down at (column, row) for a frame, then lifted
void tap(ControlTabs &tabs, int column, int row) {
  tabs.handle({.touch = {{column, row}}});
  tabs.handle({});
}

// The stylus put down at `from`, dragged across the row through `columns`
// (a frame at each), then lifted
void swipe(ControlTabs &tabs, std::pair<int, int> from, const std::vector<int> &columns) {
  tabs.handle({.touch = from});
  for (int column : columns)
    tabs.handle({.touch = {{column, from.second}}});
  tabs.handle({});
}

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
  panel.tabs.handle(press({.down = true}));
  panel.tabs.handle(press({.down = true}));
  EXPECT_EQ(panel.tabs.row(), 2u);
  panel.tabs.handle(press({.down = true})); // the last
  EXPECT_EQ(panel.tabs.row(), 2u);
  panel.tabs.handle(press({.r = true}));
  EXPECT_EQ(panel.tabs.tab(), 1u);
  EXPECT_EQ(panel.tabs.row(), 0u);
  // The shoulder buttons go round: past the last tab the first, and back
  panel.tabs.handle(press({.r = true}));
  EXPECT_EQ(panel.tabs.tab(), 0u);
  EXPECT_EQ(panel.tabs.row(), 2u); // (each tab keeps its row)
  panel.tabs.handle(press({.l = true}));
  EXPECT_EQ(panel.tabs.tab(), 1u);
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
// Holding the stylus down is one touch, not a new one each frame.
TEST(ControlTabsTest, TouchChoosesTabsAndControls) {
  Panel panel;
  tap(panel.tabs, 10, 0); // " Ragdoll " starts at column 7
  EXPECT_EQ(panel.tabs.tab(), 1u);
  tap(panel.tabs, 3, 4); // the third control's row
  EXPECT_EQ(panel.tabs.row(), 2u);
  tap(panel.tabs, 3, 20); // under the list: nothing
  EXPECT_EQ(panel.tabs.row(), 2u);
  panel.tabs.handle({.touch = {{3, 2}}});
  panel.tabs.handle({.touch = {{3, 2}}});
  panel.tabs.handle({.touch = {{3, 3}}}); // held, and slid down a row: still the first
  EXPECT_EQ(panel.tabs.row(), 0u);
  EXPECT_TRUE(panel.sent.empty());
}

// Tapping a boolean's row chooses it and toggles it, as A does; tapping
// another control's row only chooses it.
TEST(ControlTabsTest, TappingABooleanTogglesIt) {
  Panel panel;
  tap(panel.tabs, 20, 4); // mute (master, music, mute)
  EXPECT_EQ(panel.tabs.row(), 2u);
  ASSERT_EQ(panel.sent.size(), 1u);
  EXPECT_EQ(panel.sent[0].first, "mute");
  EXPECT_TRUE(std::get<ControlType::Boolean>(panel.sent[0].second).value);
  EXPECT_EQ(panel.text(4).substr(29), "[x]");
  tap(panel.tabs, 29, 4);
  EXPECT_FALSE(std::get<ControlType::Boolean>(panel.sent.back().second).value);
  tap(panel.tabs, 20, 2); // master: chosen, not changed
  EXPECT_EQ(panel.tabs.row(), 0u);
  EXPECT_EQ(panel.sent.size(), 2u);
  swipe(panel.tabs, {20, 4}, {22, 25}); // swiping a boolean neither steps nor toggles it
  EXPECT_EQ(panel.sent.size(), 2u);
}

// Swiping a number's row steps it a step a column the stylus moves, from
// where it came down, and back again; a ranged number crosses its range in
// kSwipeSteps columns. A string with allowed values steps through them.
TEST(ControlTabsTest, SwipingStepsAControl) {
  Panel panel;
  swipe(panel.tabs, {10, 3}, {9, 7, 6, 8}); // music, 0.5: down 4 steps of 0.05, back 2
  const auto music = [&] { return std::get<ControlType::Double>(panel.sent.back().second).value; };
  EXPECT_EQ(panel.sent.size(), 4u); // one change for each column that moved
  EXPECT_NEAR(music(), 0.4, 1e-9);
  EXPECT_EQ(panel.text(3).substr(28), "0.40");
  swipe(panel.tabs, {10, 3}, {40, 0}); // off the ends: held to its range
  EXPECT_NEAR(music(), 0.0, 1e-9);

  panel.tabs.handle({.r = true});
  swipe(panel.tabs, {5, 3}, {6}); // Steps a second: "30" to "60"
  EXPECT_EQ(std::get<ControlType::String>(panel.sent.back().second).value, "60");

  ControlTabs wide({}, 32, 24);
  wide.show(snapshotOf("MidiPadd", {{"note", ControlType::Integer(64, {{0, 127}})}}),
            PaddAddress{});
  swipe(wide, {10, 2}, {12}); // 2 columns of 127 / 20 (6, as a whole number)
  EXPECT_NE(wide.lines()[2][0].text.find("76"), std::string::npos);
  wide.handle({.right = true}); // the D-pad still steps by 1
  EXPECT_NE(wide.lines()[2][0].text.find("77"), std::string::npos);
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
  EXPECT_EQ(describe(ControlType::Info("logs/app.log")), "logs/app.log");
}

// Information too long for its row (an Info control: a log's path) gets
// its name on one row and its text wrapped on the rows below, indented;
// short information sits at the right like any value. Choosing it (up and
// down, or a tap on any of its rows) marks its first row, and stepping it
// changes nothing.
TEST(ControlTabsTest, LongInformationWrapsOntoTheRowsBelow) {
  std::vector<std::string> sent;
  ControlTabs tabs(
      [&](PaddAddress, const MessageType::ControlChange &change) {
        sent.push_back(change.controlName);
      },
      32, 24);
  const std::string path = "fat:/data/cyclone-ds-ragdoll/logs/cyclone-ds-ragdoll.log"; // 56
  tabs.show(snapshotOf("StoragePadd", {{"a log", ControlType::Info(path)},
                                       {"b syncs", ControlType::Info("16")},
                                       {"c batch", ControlType::Integer(1, {{1, 256}})}}),
            PaddAddress{});
  auto text = [&](int row) {
    std::string joined;
    const std::vector<Line> lines = tabs.lines();
    for (const Segment &segment : lines[static_cast<size_t>(row)])
      joined += segment.text;
    return joined;
  };
  EXPECT_EQ(text(2), "> a log");
  EXPECT_EQ(text(3), "    " + path.substr(0, 28));
  EXPECT_EQ(text(4), "    " + path.substr(28));
  EXPECT_EQ(text(5).substr(0, 9), "  b syncs");
  EXPECT_EQ(text(5).substr(30), "16");
  EXPECT_EQ(text(6).substr(0, 9), "  c batch");

  tabs.handle({.right = true}); // information: nothing to step
  EXPECT_TRUE(sent.empty());
  tabs.handle({.down = true});
  EXPECT_EQ(tabs.row(), 1u);
  EXPECT_EQ(text(5).substr(0, 9), "> b syncs");
  tap(tabs, 6, 4); // the log's second wrapped row
  EXPECT_EQ(tabs.row(), 0u);
  tap(tabs, 6, 6);
  EXPECT_EQ(tabs.row(), 2u);
}

// A chosen control whose text wraps is scrolled into view whole
TEST(ControlTabsTest, ScrollsToShowAllOfWrappedInformation) {
  ControlTabs tabs({}, 32, 8); // 5 rows for controls
  ControlValueMap controls;
  for (char c = 'a'; c <= 'e'; ++c)
    controls.emplace(std::string("control ") + c, ControlType::Integer(0));
  controls.emplace("z info", ControlType::Info(std::string(56, 'x'))); // 3 rows: name, 2 of text
  tabs.show(snapshotOf("First", controls), PaddAddress{});
  for (int i = 0; i < 5; ++i)
    tabs.handle({.down = true});
  ASSERT_EQ(tabs.row(), 5u);
  const std::vector<Line> lines = tabs.lines();
  EXPECT_EQ(lines[4][0].text, "> z info");
  EXPECT_EQ(lines[6][0].text, "    " + std::string(28, 'x'));
}

// The panel's input from the pad: what was pressed since the last frame's
// copy (Nintendo's A, the right face button, is the panel's A), and the
// stylus in 8-pixel text cells while it's down
TEST(ControlTabsTest, InputFromThePad) {
  using ScratchPadd::Input::Button;
  ScratchPadd::Input::Gamepad before, now;
  now.presses[static_cast<std::size_t>(Button::FaceRight)] = 1;
  now.presses[static_cast<std::size_t>(Button::Down)] = 2;
  now.held = ScratchPadd::Input::bitOf(Button::L); // held from before: not a press
  now.touch = {true, 17, 30};
  const PanelInput input = panelInputFrom(before, now);
  EXPECT_TRUE(input.a);
  EXPECT_TRUE(input.down);
  EXPECT_FALSE(input.l);
  EXPECT_FALSE(input.select);
  EXPECT_EQ(input.touch, std::optional(std::make_pair(2, 3)));
  EXPECT_FALSE(panelInputFrom(now, now).a); // nothing new
  now.touch = {};
  EXPECT_FALSE(panelInputFrom(before, now).touch);
}

// Each row says what it shows, for its colour: settings (controls that can
// be changed), information (Info controls, wrapped rows included), or the
// rest (the tabs, the help)
TEST(ControlTabsTest, RowsSayWhetherTheyCanBeChanged) {
  ControlTabs tabs({}, 32, 24);
  tabs.show(snapshotOf("Padd", {{"a info", ControlType::Info(std::string(40, 'x'))},
                                {"b volume", ControlType::Double(0.5, {{0.0, 1.0}})}}),
            PaddAddress{});
  const std::vector<Line> lines = tabs.lines();
  EXPECT_EQ(lines[0][0].shows, Shows::Other); // the tab
  EXPECT_EQ(lines[2][0].shows, Shows::Information);
  EXPECT_TRUE(lines[2][0].highlighted);
  EXPECT_EQ(lines[3][0].shows, Shows::Information); // its wrapped text
  EXPECT_EQ(lines[4][0].shows, Shows::Information);
  EXPECT_EQ(lines[5][0].shows, Shows::Setting);
  EXPECT_FALSE(lines[5][0].highlighted);
  EXPECT_EQ(lines[23][0].shows, Shows::Other); // the help
}

// Information in lines of tab-separated columns is a table: the name on its
// row, then each line below, the columns lined up
TEST(ControlTabsTest, InformationInColumnsIsATable) {
  ControlTabs tabs({}, 32, 24);
  tabs.show(snapshotOf("About", {{"Padds", ControlType::Info("Padd\tRuns\nGamePadd\tOnMainThread\n"
                                                             "RagdollPadd\tSteadily")}}),
            PaddAddress{});
  auto text = [&](int row) { return tabs.lines()[static_cast<size_t>(row)][0].text; };
  EXPECT_EQ(text(2), "> Padds");
  EXPECT_EQ(text(3), "    Padd        Runs");
  EXPECT_EQ(text(4), "    GamePadd    OnMainThread");
  EXPECT_EQ(text(5), "    RagdollPadd Steadily");
}
