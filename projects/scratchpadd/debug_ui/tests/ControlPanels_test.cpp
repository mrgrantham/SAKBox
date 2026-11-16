// A window's controls (ControlPanels), without drawing them: a tab per padd
// in the order they first answered, updated in place, removed when the padd
// has no controls left; labelled without "Padd"; the window opening at the
// bottom left unless told otherwise.
#include <ScratchPadd/DebugUi/ControlPanel.hpp>

#include "implot.h"
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
  DebugUi::ControlPanels panels(
      [](PaddAddress, const std::string &, const MessageType::ControlChange &) {});
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

  const DebugUi::ControlPanels bottomLeft(
      [](PaddAddress, const std::string &, const MessageType::ControlChange &) {});
  EXPECT_EQ(bottomLeft.placement().x, 0.0f);
  EXPECT_EQ(bottomLeft.placement().y, 1.0f);
  const DebugUi::ControlPanels topRight(
      [](PaddAddress, const std::string &, const MessageType::ControlChange &) {}, ImVec2(1, 0));
  EXPECT_EQ(topRight.placement().x, 1.0f);
  EXPECT_EQ(topRight.placement().y, 0.0f);
}

// A panel draws every kind of control, graphs as lines and bars included,
// in a frame of ImGui with no window behind it (the drawing is made, not
// shown), and drawing a graph changes nothing
TEST(ControlPanelTest, DrawsEveryKindOfControlGraphsIncluded) {
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2(800, 600);
  unsigned char *pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height); // (builds the font)

  ControlType::Graph line(60, ControlType::Graph::Style::Line, std::nullopt, " ms");
  ControlType::Graph bars(60, ControlType::Graph::Style::Bars, {{0.0f, 100.0f}}, "%");
  for (int i = 0; i < 30; ++i) {
    line.add(static_cast<float>(i % 7));
    bars.add(static_cast<float>(i * 3));
  }
  DebugUi::ControlPanel panel;
  int changes = 0;
  panel.setOnChange([&](const MessageType::ControlChange &) { ++changes; });
  panel.setControls(snapshotOf("Activity", {{"Frame time", line},
                                            {"Busy", bars},
                                            {"Empty graph", ControlType::Graph()},
                                            {"Note", ControlType::Info("a\tb\n1\t2")},
                                            {"Steps", ControlType::Integer(4)}}));
  // (two frames: ImGui measures a new window on its first, drawing nothing)
  for (int frame = 0; frame < 2; ++frame) {
    io.DeltaTime = 1.0f / 60;
    ImGui::NewFrame();
    ImGui::Begin("Controls");
    panel.draw();
    ImGui::End();
    ImGui::Render();
  }
  EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
  EXPECT_EQ(changes, 0);
  EXPECT_EQ(std::get<ControlType::Graph>(panel.controls().controlMap.at("Busy")).values,
            bars.values);
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
}
