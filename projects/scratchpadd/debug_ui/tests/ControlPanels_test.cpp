// A window's controls (ControlPanels), without drawing them: a tab per padd
// in the order they first answered, updated in place, removed when the padd
// has no controls left; labelled without "Padd"; the window opening at the
// bottom left unless told otherwise.
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

TEST(ControlPanelsTest, ATabPerPaddInTheOrderTheyAnswered) {
  DebugUi::ControlPanels panels([](PaddAddress, const MessageType::ControlChange &) {});
  panels.show(snapshotOf("Physics", {{"Steps", ControlType::Integer(60)}}), PaddAddress{});
  panels.show(snapshotOf("About", {{"App", ControlType::Info("demo")}}), PaddAddress{});
  panels.show(snapshotOf("Audio", {{"Volume", ControlType::Double(0.5)}}), PaddAddress{});
  EXPECT_EQ(panels.tabNames(), (std::vector<std::string>{"Physics", "About", "Audio"}));
  panels.show(snapshotOf("Physics", {{"Steps", ControlType::Integer(30)}}), PaddAddress{});
  EXPECT_EQ(panels.size(), 3u); // updated in place, where it was
  EXPECT_EQ(panels.tabNames().front(), "Physics");
  panels.show(snapshotOf("About", {}), PaddAddress{});
  EXPECT_EQ(panels.tabNames(), (std::vector<std::string>{"Physics", "Audio"}));
  panels.show(snapshotOf("Nobody", {}), PaddAddress{}); // nothing to remove
  EXPECT_EQ(panels.size(), 2u);
}

TEST(ControlPanelsTest, TabsLabelledWithoutPaddAndTheWindowPlaced) {
  EXPECT_EQ(DebugUi::ControlPanels::tabLabel("RagdollPadd"), "Ragdoll");
  EXPECT_EQ(DebugUi::ControlPanels::tabLabel("StoragePadd"), "Storage");
  EXPECT_EQ(DebugUi::ControlPanels::tabLabel("About"), "About");
  EXPECT_EQ(DebugUi::ControlPanels::tabLabel("Padd"), "Padd"); // (not to nothing)

  const DebugUi::ControlPanels bottomLeft([](PaddAddress, const MessageType::ControlChange &) {});
  EXPECT_EQ(bottomLeft.placement().x, 0.0f);
  EXPECT_EQ(bottomLeft.placement().y, 1.0f);
  const DebugUi::ControlPanels topRight([](PaddAddress, const MessageType::ControlChange &) {},
                                        ImVec2(1, 0));
  EXPECT_EQ(topRight.placement().x, 1.0f);
  EXPECT_EQ(topRight.placement().y, 0.0f);
}
