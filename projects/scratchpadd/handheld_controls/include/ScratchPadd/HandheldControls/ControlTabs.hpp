// ControlTabs.hpp - every padd's controls on a handheld's text screen: a row
// of tabs across the top, one for each padd with controls, and the chosen
// padd's controls listed under it, driven by the D-pad, A, L and R, and the
// stylus. It speaks the same controls protocol as the desktop's panels
// (DebugUi::ControlPanels): shown snapshots, edits sent to their owner. It
// does no drawing or input itself: ConsolePanel.hpp prints its lines and
// reads the buttons on the DS and 3DS, so this part is the same everywhere
// (and tested on desktops).
#pragma once

#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/PaddAddress.hpp>

#include <ScratchPadd/Input/Gamepad.hpp>
#include <cstddef>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::HandheldControls {

// This frame's presses (each true for the frame it went down), and where
// the stylus is, in text cells (column, row from 0), every frame it's on
// the screen
struct PanelInput {
  bool up = false, down = false, left = false, right = false;
  bool a = false, l = false, r = false;
  bool select = false; // shows or hides the panel
  std::optional<std::pair<int, int>> touch;
};

// The panel's input from the pad: what was pressed since `before` (the
// last frame's copy), and the stylus in 8-pixel text cells. The panel's
// A is Nintendo's A, the right face button.
PanelInput panelInputFrom(const Input::Gamepad &before, const Input::Gamepad &now);

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
  // and down (or a touch on a row) choose a control, left and right step it
  // (a number through its range, a string through its allowed values, a
  // boolean off and on), and A or a tap toggles a boolean. Swiping a row
  // left or right steps it a step a column the stylus moves (a ranged
  // number crosses its range in about kSwipeSteps). SELECT shows or hides it.
  // Returns whether anything shown changed (so it's redrawn only then:
  // formatting numbers is float work, slow on the DS).
  bool handle(const PanelInput &input);

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
  // twentieth of it: an integer by 1, or by a twentieth when swiped)
  static constexpr double kStepWithoutRange = 0.1;
  static constexpr int kSwipeSteps = 20;

private:
  struct Tab {
    MessageType::ControlSnapshot controls;
    PaddAddress owner;
    std::vector<std::string> names; // sorted
    std::size_t row = 0;
  };

  // A touch on the list's row, from where it came down: the control it's on
  // (if any) and the column it has stepped it to
  struct Stroke {
    int column = 0;
    std::optional<std::string> control;
    bool moved = false;
  };

  // How far a step moves an integer: by 1 for the D-pad, further for a swipe
  enum class Pace { Press, Swipe };

  // Steps the chosen control `steps` (negative for left) and sends the change
  void step(int steps, Pace pace);
  void toggle();
  void touchAt(int column, int row);
  void lift();
  void change(Tab &tab, const std::string &name, const ControlTypeVariant &value);
  Line tabRow() const;
  // The tabs' columns, as tabRow() lays them out, for taps
  std::vector<std::pair<int, int>> tabSpans() const;
  // The list laid out a row a line, unscrolled: each control's row, "name
  // ..... value", or for information too long for one (an Info control), its
  // name and then its text wrapped onto the rows below
  struct ListRow {
    std::size_t control; // which of the tab's names
    bool first;          // its first row (the one marked when chosen)
    std::string text;    // without the marker
  };
  std::vector<ListRow> listRows(const Tab &tab) const;
  // The list row at the top of the screen: far enough down to show all of
  // the chosen control's rows
  std::size_t firstRowShown() const;

  Send send_;
  int columns_, rows_;
  std::vector<Tab> tabs_; // in the order their padds first answered
  std::size_t tab_ = 0;
  bool visible_ = true;
  bool dirty_ = true;
  std::optional<Stroke> stroke_; // while the stylus is down
};

// A control's value as the panel shows it: numbers to a few places,
// booleans as [x] or [ ], strings as they are
std::string describe(const ControlTypeVariant &value);

} // namespace ScratchPadd::HandheldControls
