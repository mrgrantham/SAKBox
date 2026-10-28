// ControlTabs.hpp - every padd's controls on a handheld's text screen: a row
// of tabs across the top, one for each padd with controls, and the chosen
// padd's controls listed under it, driven by the D-pad, A, L and R, and the
// touch screen. It speaks the same controls protocol as the desktop's panels
// (DebugUi::ControlPanels): shown snapshots, edits sent to their owner. It
// does no drawing or input itself: ConsolePanel.hpp prints its lines and
// reads the buttons on the DS and 3DS, so this part is the same everywhere
// (and tested on desktops).
#pragma once

#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/PaddAddress.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::HandheldControls {

// This frame's presses (each true for the frame it went down), and where
// the screen was touched, in text cells (column, row from 0)
struct Input {
  bool up = false, down = false, left = false, right = false;
  bool a = false, l = false, r = false;
  bool select = false; // shows or hides the panel
  std::optional<std::pair<int, int>> touched;
};

// A run of text in one line, highlighted or not (the chosen tab or row)
struct Segment {
  std::string text;
  bool highlighted = false;
};
using Line = std::vector<Segment>;

class ControlTabs {
public:
  // Sends `change` to `owner` (the padd's own sendTo)
  using Send = std::function<void(PaddAddress owner, const MessageType::ControlChange &change)>;

  // A panel `columns` x `rows` text cells (the DS's console is 32 x 24, the
  // 3DS's bottom screen 40 x 30)
  ControlTabs(Send send, int columns, int rows);

  // Shows `snapshot` as its padd's tab: a new tab the first time, updated
  // after that (keeping the chosen row), gone when the padd has no controls
  void show(const MessageType::ControlSnapshot &snapshot, PaddAddress owner);

  // Acts on this frame's input: L and R (or a tap on a tab) change tabs, up
  // and down (or a tap on a row) choose a control, left and right step it
  // (a number through its range, a string through its allowed values, a
  // boolean off and on), and A toggles a boolean. SELECT shows or hides it.
  // Returns whether anything shown changed (so it's redrawn only then:
  // formatting numbers is float work, slow on the DS).
  bool handle(const Input &input);

  bool visible() const { return visible_; }
  void setVisible(bool visible);
  // Whether it needs drawing (it changed since the last lines() taken)
  bool dirty() const { return dirty_; }

  // What to show, a line a row (rows() of them, each at most columns()
  // wide); taking them marks it drawn
  std::vector<Line> lines();
  int columns() const { return columns_; }
  int rows() const { return rows_; }

  // For tests and apps: the tabs' padd names, the chosen tab and row
  std::vector<std::string> tabNames() const;
  std::size_t tab() const { return tab_; }
  std::size_t row() const;

  // How far left and right step a number without a range (with one, a
  // twentieth of it)
  static constexpr double kStepWithoutRange = 0.1;

private:
  struct Tab {
    MessageType::ControlSnapshot controls;
    PaddAddress owner;
    std::vector<std::string> names; // sorted
    std::size_t row = 0;
  };

  // Steps the chosen control `direction` (-1 or 1) and sends the change
  void step(int direction);
  void toggle();
  void change(Tab &tab, const std::string &name, const ControlTypeVariant &value);
  Line tabRow() const;
  // The tabs' columns, as tabRow() lays them out, for taps
  std::vector<std::pair<int, int>> tabSpans() const;
  std::size_t firstRowShown() const;

  Send send_;
  int columns_, rows_;
  std::vector<Tab> tabs_; // in the order their padds first answered
  std::size_t tab_ = 0;
  bool visible_ = true;
  bool dirty_ = true;
};

// A control's value as the panel shows it: numbers to a few places,
// booleans as [x] or [ ], strings as they are
std::string describe(const ControlTypeVariant &value);

} // namespace ScratchPadd::HandheldControls
