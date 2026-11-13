#include <ScratchPadd/HandheldControls/ControlTabs.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace ScratchPadd::HandheldControls {

namespace {

// Rows the panel uses besides its list: the tabs, a rule under them, and the
// help at the bottom
constexpr int kTabRow = 0, kFirstListRow = 2, kRowsBesidesList = 3;

// A tab's text: its padd's name, without a trailing "Padd" (AudioPadd's tab
// is "Audio"), in brackets when chosen
std::string tabLabel(const std::string &paddName, bool chosen) {
  std::string name = paddName;
  if (name.size() > 4 && name.ends_with("Padd"))
    name.resize(name.size() - 4);
  return chosen ? "[" + name + "]" : " " + name + " ";
}

} // namespace

std::string describe(const ControlTypeVariant &value) {
  char text[32];
  if (const auto *d = std::get_if<ControlType::Double>(&value)) {
    std::snprintf(text, sizeof text, "%.2f", d->value);
    return text;
  }
  if (const auto *i = std::get_if<ControlType::Integer>(&value))
    return std::to_string(i->value);
  if (const auto *b = std::get_if<ControlType::Boolean>(&value))
    return b->value ? "[x]" : "[ ]";
  return std::get<ControlType::String>(value).value;
}

ControlTabs::ControlTabs(Send send, int columns, int rows)
    : send_(std::move(send)), columns_(columns), rows_(rows) {}

void ControlTabs::show(const MessageType::ControlSnapshot &snapshot, PaddAddress owner) {
  auto existing = std::ranges::find_if(
      tabs_, [&](const Tab &tab) { return tab.controls.paddName == snapshot.paddName; });
  dirty_ = true;
  if (snapshot.controlMap.empty()) {
    if (existing != tabs_.end()) {
      const auto index = static_cast<std::size_t>(existing - tabs_.begin());
      tabs_.erase(existing);
      if (tab_ > index || tab_ >= tabs_.size())
        tab_ = tab_ > 0 ? tab_ - 1 : 0;
    }
    return;
  }
  Tab &tab = existing != tabs_.end() ? *existing : tabs_.emplace_back();
  tab.controls = snapshot;
  tab.owner = owner;
  tab.names.clear();
  for (const auto &[name, value] : snapshot.controlMap)
    tab.names.push_back(name);
  std::ranges::sort(tab.names);
  tab.row = std::min(tab.row, tab.names.size() - 1);
}

void ControlTabs::setVisible(bool visible) {
  if (visible != visible_)
    dirty_ = true;
  visible_ = visible;
}

std::vector<std::string> ControlTabs::tabNames() const {
  std::vector<std::string> names;
  names.reserve(tabs_.size());
  for (const Tab &tab : tabs_)
    names.push_back(tab.controls.paddName);
  return names;
}

std::size_t ControlTabs::row() const { return tabs_.empty() ? 0 : tabs_[tab_].row; }

PanelInput panelInputFrom(const Input::Gamepad &before, const Input::Gamepad &now) {
  using Input::Button;
  const Input::Edges edges(before, now);
  PanelInput input{.up = edges.pressed(Button::Up),
                   .down = edges.pressed(Button::Down),
                   .left = edges.pressed(Button::Left),
                   .right = edges.pressed(Button::Right),
                   .a = edges.pressed(Button::FaceRight),
                   .l = edges.pressed(Button::L),
                   .r = edges.pressed(Button::R),
                   .select = edges.pressed(Button::Select)};
  if (now.touch.down)
    input.touch = {{now.touch.x / 8, now.touch.y / 8}};
  return input;
}

bool ControlTabs::handle(const PanelInput &input) {
  if (input.select)
    setVisible(!visible_);
  if (!visible_ || tabs_.empty()) {
    stroke_.reset();
    return dirty_;
  }
  Tab &tab = tabs_[tab_];
  if (input.l && tab_ > 0) {
    --tab_;
    dirty_ = true;
  } else if (input.r && tab_ + 1 < tabs_.size()) {
    ++tab_;
    dirty_ = true;
  } else if (input.up && tab.row > 0) {
    --tab.row;
    dirty_ = true;
  } else if (input.down && tab.row + 1 < tab.names.size()) {
    ++tab.row;
    dirty_ = true;
  } else if (input.left || input.right) {
    step(input.right ? 1 : -1, Pace::Press);
  } else if (input.a) {
    toggle();
  }
  if (input.touch)
    touchAt(input.touch->first, input.touch->second);
  else if (stroke_)
    lift();
  return dirty_;
}

void ControlTabs::touchAt(int column, int row) {
  if (stroke_) {
    // A swipe: a step for each column moved since the last, on the control
    // it came down on (booleans are tapped, not swiped)
    const int steps = column - stroke_->column;
    if (steps == 0 || !stroke_->control)
      return;
    stroke_->column = column;
    stroke_->moved = true;
    const Tab &tab = tabs_[tab_];
    if (tab.names[tab.row] == *stroke_->control &&
        !std::holds_alternative<ControlType::Boolean>(
            tab.controls.controlMap.at(tab.names[tab.row])))
      step(steps, Pace::Swipe);
    return;
  }
  // The stylus just came down: on a tab, it's chosen; on a row, its control
  stroke_ = Stroke{.column = column};
  if (row == kTabRow) {
    const auto spans = tabSpans();
    for (std::size_t i = 0; i < spans.size(); ++i) {
      if (column >= spans[i].first && column < spans[i].second && i != tab_) {
        tab_ = i;
        dirty_ = true;
      }
    }
  } else if (row >= kFirstListRow && row < rows_ - 1) {
    Tab &tab = tabs_[tab_];
    const std::size_t chosen = firstRowShown() + static_cast<std::size_t>(row - kFirstListRow);
    if (chosen < tab.names.size()) {
      if (chosen != tab.row)
        dirty_ = true;
      tab.row = chosen;
      stroke_->control = tab.names[chosen];
    }
  }
}

void ControlTabs::lift() {
  // A tap (down and up without moving) on a boolean toggles it
  const std::optional<Stroke> stroke = std::exchange(stroke_, std::nullopt);
  const Tab &tab = tabs_[tab_];
  if (stroke && stroke->control && !stroke->moved && tab.names[tab.row] == *stroke->control)
    toggle();
}

void ControlTabs::step(int steps, Pace pace) {
  Tab &tab = tabs_[tab_];
  const std::string &name = tab.names[tab.row];
  ControlTypeVariant value = tab.controls.controlMap.at(name);
  bool changed = false;
  if (auto *d = std::get_if<ControlType::Double>(&value)) {
    const double size = d->enforcableRange
                            ? (d->enforcableRange->second - d->enforcableRange->first) / kSwipeSteps
                            : kStepWithoutRange;
    double next = d->value + steps * size;
    if (d->enforcableRange)
      next = std::clamp(next, d->enforcableRange->first, d->enforcableRange->second);
    changed = next != d->value;
    d->value = next;
  } else if (auto *i = std::get_if<ControlType::Integer>(&value)) {
    std::int64_t size = 1;
    if (pace == Pace::Swipe && i->enforcableRange)
      size = std::max<std::int64_t>(
          1, (std::int64_t{i->enforcableRange->second} - i->enforcableRange->first) / kSwipeSteps);
    std::int64_t next = i->value + steps * size;
    if (i->enforcableRange)
      next = std::clamp<std::int64_t>(next, i->enforcableRange->first, i->enforcableRange->second);
    next = std::clamp<std::int64_t>(next, INT32_MIN, INT32_MAX);
    changed = next != i->value;
    i->value = static_cast<int32_t>(next);
  } else if (auto *b = std::get_if<ControlType::Boolean>(&value)) {
    changed = b->value != (steps > 0);
    b->value = steps > 0;
  } else if (auto *s = std::get_if<ControlType::String>(&value)) {
    // Through its allowed values, round and round; free text isn't edited
    if (s->enforcableRange && s->enforcableRange->size() > 1) {
      const auto &options = *s->enforcableRange;
      const auto at = std::ranges::find(options, s->value);
      const auto index =
          at == options.end() ? 0 : static_cast<std::ptrdiff_t>(at - options.begin());
      const auto count = static_cast<std::ptrdiff_t>(options.size());
      const std::string next =
          options[static_cast<std::size_t>(((index + steps) % count + count) % count)];
      changed = next != s->value;
      s->value = next;
    }
  }
  if (changed)
    change(tab, name, value);
}

void ControlTabs::toggle() {
  Tab &tab = tabs_[tab_];
  const std::string &name = tab.names[tab.row];
  ControlTypeVariant value = tab.controls.controlMap.at(name);
  if (auto *b = std::get_if<ControlType::Boolean>(&value)) {
    b->value = !b->value;
    change(tab, name, value);
  }
}

void ControlTabs::change(Tab &tab, const std::string &name, const ControlTypeVariant &value) {
  // Shown at once; the owner's reply (a new snapshot) confirms or corrects it
  tab.controls.controlMap.at(name) = value;
  dirty_ = true;
  if (send_)
    send_(tab.owner, MessageType::ControlChange{.controlName = name, .controlTypeValue = value});
}

std::vector<std::pair<int, int>> ControlTabs::tabSpans() const {
  // Laid out from the first tab, unless that leaves the chosen one off the
  // right edge: then from the first that keeps it on
  std::vector<int> widths;
  widths.reserve(tabs_.size());
  for (std::size_t i = 0; i < tabs_.size(); ++i)
    widths.push_back(static_cast<int>(tabLabel(tabs_[i].controls.paddName, i == tab_).size()));
  std::size_t first = 0;
  auto widthFrom = [&](std::size_t from) {
    int width = 0;
    for (std::size_t i = from; i <= tab_ && i < widths.size(); ++i)
      width += widths[i];
    return width;
  };
  while (first < tab_ && widthFrom(first) > columns_)
    ++first;
  std::vector<std::pair<int, int>> spans(tabs_.size(), {-1, -1});
  int column = 0;
  for (std::size_t i = first; i < tabs_.size() && column < columns_; ++i) {
    spans[i] = {column, std::min(column + widths[i], columns_)};
    column += widths[i];
  }
  return spans;
}

Line ControlTabs::tabRow() const {
  Line line;
  const auto spans = tabSpans();
  for (std::size_t i = 0; i < tabs_.size(); ++i) {
    if (spans[i].first < 0)
      continue;
    const std::string label = tabLabel(tabs_[i].controls.paddName, i == tab_);
    line.push_back(
        {label.substr(0, static_cast<std::size_t>(spans[i].second - spans[i].first)), i == tab_});
  }
  return line;
}

std::size_t ControlTabs::firstRowShown() const {
  const auto shown = static_cast<std::size_t>(std::max(rows_ - kRowsBesidesList, 1));
  const std::size_t chosen = row();
  return chosen < shown ? 0 : chosen - shown + 1;
}

std::vector<Line> ControlTabs::lines() {
  dirty_ = false;
  std::vector<Line> lines(static_cast<std::size_t>(rows_));
  const auto width = static_cast<std::size_t>(columns_);
  if (tabs_.empty()) {
    lines[kTabRow] = {{"Controls: none yet"}};
  } else {
    lines[kTabRow] = tabRow();
    const Tab &tab = tabs_[tab_];
    const std::size_t first = firstRowShown();
    for (int r = kFirstListRow; r < rows_ - 1; ++r) {
      const std::size_t index = first + static_cast<std::size_t>(r - kFirstListRow);
      if (index >= tab.names.size())
        break;
      const std::string &name = tab.names[index];
      const std::string value = describe(tab.controls.controlMap.at(name));
      // "> name ....... value", the value at the right
      const std::size_t room = width > value.size() + 3 ? width - value.size() - 3 : 0;
      std::string text = (index == tab.row ? "> " : "  ") + name.substr(0, room);
      text.append(width > text.size() + value.size() ? width - text.size() - value.size() : 1, ' ');
      text += value;
      lines[static_cast<std::size_t>(r)] = {{text.substr(0, width), index == tab.row}};
    }
  }
  lines[1] = {{std::string(width, '-')}};
  lines[static_cast<std::size_t>(rows_ - 1)] = {
      {std::string("L/R tab ^v pick <> set A tick").substr(0, width)}};
  return lines;
}

} // namespace ScratchPadd::HandheldControls
