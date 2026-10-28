#include <ScratchPadd/HandheldControls/ControlTabs.hpp>

#include <algorithm>
#include <cstdio>

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

bool ControlTabs::handle(const Input &input) {
  if (input.select)
    setVisible(!visible_);
  if (!visible_ || tabs_.empty())
    return dirty_;
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
    step(input.right ? 1 : -1);
  } else if (input.a) {
    toggle();
  }
  if (input.touched) {
    const auto [column, row] = *input.touched;
    if (row == kTabRow) {
      const auto spans = tabSpans();
      for (std::size_t i = 0; i < spans.size(); ++i) {
        if (column >= spans[i].first && column < spans[i].second && i != tab_) {
          tab_ = i;
          dirty_ = true;
        }
      }
    } else if (row >= kFirstListRow && row < rows_ - 1) {
      Tab &touchedTab = tabs_[tab_];
      const std::size_t chosen = firstRowShown() + static_cast<std::size_t>(row - kFirstListRow);
      if (chosen < touchedTab.names.size() && chosen != touchedTab.row) {
        touchedTab.row = chosen;
        dirty_ = true;
      }
    }
  }
  return dirty_;
}

void ControlTabs::step(int direction) {
  Tab &tab = tabs_[tab_];
  const std::string &name = tab.names[tab.row];
  ControlTypeVariant value = tab.controls.controlMap.at(name);
  bool changed = false;
  if (auto *d = std::get_if<ControlType::Double>(&value)) {
    const double size = d->enforcableRange
                            ? (d->enforcableRange->second - d->enforcableRange->first) / 20
                            : kStepWithoutRange;
    double next = d->value + direction * size;
    if (d->enforcableRange)
      next = std::clamp(next, d->enforcableRange->first, d->enforcableRange->second);
    changed = next != d->value;
    d->value = next;
  } else if (auto *i = std::get_if<ControlType::Integer>(&value)) {
    int32_t next = i->value + direction;
    if (i->enforcableRange)
      next = std::clamp(next, i->enforcableRange->first, i->enforcableRange->second);
    changed = next != i->value;
    i->value = next;
  } else if (auto *b = std::get_if<ControlType::Boolean>(&value)) {
    changed = b->value != (direction > 0);
    b->value = direction > 0;
  } else if (auto *s = std::get_if<ControlType::String>(&value)) {
    // Through its allowed values, round and round; free text isn't edited
    if (s->enforcableRange && !s->enforcableRange->empty()) {
      const auto &options = *s->enforcableRange;
      const auto at = std::ranges::find(options, s->value);
      const auto index =
          at == options.end() ? 0 : static_cast<std::ptrdiff_t>(at - options.begin());
      const auto count = static_cast<std::ptrdiff_t>(options.size());
      s->value = options[static_cast<std::size_t>(((index + direction) % count + count) % count)];
      changed = options.size() > 1;
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
