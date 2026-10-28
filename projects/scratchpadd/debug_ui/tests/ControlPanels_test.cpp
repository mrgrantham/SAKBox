// A window's control panels (ControlPanels), without drawing them: one per
// padd, updated in place, closed when the padd has no controls left, and
// placed along the bottom unless told otherwise.
#include <ScratchPadd/DebugUi/ControlPanel.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace ScratchPadd;

namespace {
MessageType::ControlSnapshot snapshotOf(const std::string &padd, ControlValueMap controls) {
  MessageType::ControlSnapshot snapshot;
  snapshot.paddName = padd;
  snapshot.controlMap = std::move(controls);
  return snapshot;
}
} // namespace

TEST(ControlPanelsTest, OnePanelPerPaddUpdatedInPlaceAndClosedWhenEmpty) {
  DebugUi::ControlPanels panels([](PaddAddress, const MessageType::ControlChange &) {});
  panels.show(snapshotOf("Audio", {{"Volume", ControlType::Double(0.5)}}), PaddAddress{});
  panels.show(snapshotOf("Physics", {{"Steps", ControlType::Integer(60)}}), PaddAddress{});
  EXPECT_EQ(panels.size(), 2u);
  panels.show(snapshotOf("Audio", {{"Volume", ControlType::Double(0.7)}}), PaddAddress{});
  EXPECT_EQ(panels.size(), 2u);
  panels.show(snapshotOf("Audio", {}), PaddAddress{});
  EXPECT_EQ(panels.size(), 1u);
  panels.show(snapshotOf("Nobody", {}), PaddAddress{}); // nothing to close
  EXPECT_EQ(panels.size(), 1u);
}

TEST(ControlPanelsTest, PanelsOpenAlongTheBottomOrWhereTold) {
  const ImVec2 first = DebugUi::ControlPanels::alongTheBottom("", 0);
  const ImVec2 fifth = DebugUi::ControlPanels::alongTheBottom("", 4);
  EXPECT_EQ(first.x, 0.0f);
  EXPECT_EQ(first.y, 1.0f);
  EXPECT_EQ(fifth.x, first.x);                                      // round again
  EXPECT_EQ(DebugUi::ControlPanels::alongTheBottom("", 3).y, 0.0f); // then the top right

  std::vector<std::string> placed;
  DebugUi::ControlPanels panels([](PaddAddress, const MessageType::ControlChange &) {},
                                [&](const std::string &padd, std::size_t) {
                                  placed.push_back(padd);
                                  return ImVec2(1, 0);
                                });
  panels.show(snapshotOf("Physics", {{"Steps", ControlType::Integer(60)}}), PaddAddress{});
  panels.show(snapshotOf("Physics", {{"Steps", ControlType::Integer(30)}}), PaddAddress{});
  EXPECT_EQ(placed, std::vector<std::string>{"Physics"}); // placed once, when it opened
}
