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

// A panel showing one padd's controls as widgets: sliders for ranged
// numbers, input boxes otherwise, a checkbox for booleans, and a combo box
// or text field for strings. Edits update the panel's copy at once and are
// reported through the change handler; the owning padd's reply (a new
// snapshot passed to setControls) confirms or corrects them.
class ControlPanel {
public:
  using ChangeHandler = std::function<void(const MessageType::ControlChange &)>;

  // Shows `controls`, replacing whatever was shown before
  void setControls(const MessageType::ControlSnapshot &controls);
  const MessageType::ControlSnapshot &controls() const { return controls_; }
  // Called with a ControlChange each time the user edits a control
  void setOnChange(ChangeHandler onChange) { onChange_ = std::move(onChange); }
  // Where the panel first appears, as a fraction of the window: (0, 0) is
  // the top-left corner, (1, 0) the top-right. Without it, ImGui picks a
  // spot. The user can still move it.
  void setInitialPlacement(ImVec2 placement) { placement_ = placement; }

  // Draws the panel, sized to fit its controls (between DebugUi's
  // newFrame and the frame's end)
  void draw();

private:
  MessageType::ControlSnapshot controls_;
  std::vector<std::string> names_; // control names, sorted
  ChangeHandler onChange_;
  std::optional<ImVec2> placement_;
};

// A window's control panels: one per padd with controls, kept from each
// ControlSnapshot it's shown. A main-thread padd that draws a window
// receives the snapshots (asking for them with a ControlRequest) and passes
// them to show(); edits go back to the padd each came from.
class ControlPanels {
public:
  // Sends `change` to `owner` (the padd's own sendTo)
  using Send = std::function<void(PaddAddress owner, const MessageType::ControlChange &change)>;
  // Where a padd's panel first opens (see ControlPanel::setInitialPlacement)
  using Placement = std::function<ImVec2(const std::string &paddName, std::size_t index)>;

  // By default, each new panel opens at the next spot along the bottom of
  // the window (left, middle, right, then top-right), so they don't all
  // open on top of each other
  explicit ControlPanels(Send send, Placement placement = alongTheBottom);

  // Shows `snapshot` in its padd's panel, opening one the first time and
  // closing it when the padd has no controls left
  void show(const MessageType::ControlSnapshot &snapshot, PaddAddress owner);
  void draw();
  void clear() { panels_.clear(); }
  std::size_t size() const { return panels_.size(); }

  static ImVec2 alongTheBottom(const std::string &paddName, std::size_t index);

private:
  Send send_;
  Placement placement_;
  std::map<std::string, std::unique_ptr<ControlPanel>> panels_; // by padd name
  std::size_t opened_ = 0;
};

} // namespace ScratchPadd::DebugUi
