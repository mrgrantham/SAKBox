// ControlPanel.hpp - one padd's controls as ImGui widgets, and the set of
// them a window shows (one panel per padd with controls).
#pragma once

#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/PaddAddress.hpp>

#include "imgui.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::DebugUi {

// One padd's controls as widgets: sliders for ranged numbers, input boxes
// otherwise, a checkbox for booleans, a combo box or text field for
// strings, and information (Info) as text or a table. Edits update the
// panel's copy at once and are reported through the change handler; the
// owning padd's reply (a new snapshot passed to setControls) confirms or
// corrects them.
class ControlPanel {
public:
  using ChangeHandler = std::function<void(const MessageType::ControlChange &)>;

  // Shows `controls`, replacing whatever was shown before
  void setControls(const MessageType::ControlSnapshot &controls);
  const MessageType::ControlSnapshot &controls() const { return controls_; }
  // Called with a ControlChange each time the user edits a control
  void setOnChange(ChangeHandler onChange) { onChange_ = std::move(onChange); }

  // Draws the controls' widgets in the current ImGui window (between
  // DebugUi's newFrame and the frame's end)
  void draw();

private:
  MessageType::ControlSnapshot controls_;
  std::vector<std::string> names_; // control names, sorted
  ChangeHandler onChange_;
};

// A window's controls: one "Controls" window with a tab for each padd with
// controls, kept from each ControlSnapshot it's shown, in the order the
// padds first answered (dragging a tab moves it). A main-thread padd that
// draws a window receives the snapshots (asking for them with a
// ControlRequest) and passes them to show(); edits go back to the padd each
// came from.
class ControlPanels {
public:
  // Sends `change` to `owner` (the padd's own sendTo)
  using Send = std::function<void(PaddAddress owner, const MessageType::ControlChange &change)>;

  // `placement`: where the window first opens, as a fraction of the
  // display: (0, 1), the default, is the bottom-left corner, (1, 0) the
  // top-right. The user can still move it.
  explicit ControlPanels(Send send, ImVec2 placement = ImVec2(0, 1));

  // Shows `snapshot` in its padd's tab, adding one the first time and
  // removing it when the padd has no controls left
  void show(const MessageType::ControlSnapshot &snapshot, PaddAddress owner);
  // The window, sized to fit the chosen tab's controls (nothing while no
  // padd has any)
  void draw();
  void clear() { tabs_.clear(); }
  std::size_t size() const { return tabs_.size(); }
  // The tabs' padd names, in order
  std::vector<std::string> tabNames() const;
  ImVec2 placement() const { return placement_; }

  // A tab's label: the padd's name without "Padd" ("RagdollPadd" ->
  // "Ragdoll"), as the handheld panel shows it
  static std::string tabLabel(const std::string &paddName);

private:
  struct Tab {
    std::string paddName;
    std::unique_ptr<ControlPanel> panel;
  };
  Send send_;
  ImVec2 placement_;
  std::vector<Tab> tabs_; // in the order the padds first answered
};

} // namespace ScratchPadd::DebugUi
