#include <ScratchPadd/DebugUi/ControlPanel.hpp>
#include <ScratchPadd/DebugUi/Theme.hpp>
#include <ScratchPadd/Helper.hpp>

#include <algorithm>
#include <cstdint>
#include <imgui_stdlib.h>
#include <string>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<int32_t, int>, "ImGui's integer widgets take int*");

namespace ScratchPadd::DebugUi {

void ControlPanel::setControls(const MessageType::ControlSnapshot &controls) {
  controls_ = controls;
  names_.clear();
  for (const auto &[name, value] : controls_.controlMap) {
    names_.push_back(name);
  }
  std::sort(names_.begin(), names_.end());
}

namespace {

// `text`'s lines, each split at its tabs
std::vector<std::vector<std::string>> cellsOf(const std::string &text) {
  std::vector<std::vector<std::string>> rows;
  for (std::size_t start = 0; start <= text.size();) {
    const std::size_t end = std::min(text.find('\n', start), text.size());
    const std::string line = text.substr(start, end - start);
    std::vector<std::string> cells;
    for (std::size_t cell = 0; cell <= line.size();) {
      const std::size_t tab = std::min(line.find('\t', cell), line.size());
      cells.push_back(line.substr(cell, tab - cell));
      cell = tab + 1;
    }
    rows.push_back(std::move(cells));
    start = end + 1;
  }
  return rows;
}

// Information in lines of tab-separated columns, as a table under its name
void drawTable(const char *name, const std::string &text) {
  const auto rows = cellsOf(text);
  std::size_t columns = 0;
  for (const auto &cells : rows)
    columns = std::max(columns, cells.size());
  ImGui::TextUnformatted(name);
  if (columns == 0 || !ImGui::BeginTable(name, static_cast<int>(columns),
                                         ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
    return;
  for (std::size_t c = 0; c < columns; ++c)
    ImGui::TableSetupColumn(c < rows[0].size() ? rows[0][c].c_str() : "");
  ImGui::TableHeadersRow();
  for (std::size_t r = 1; r < rows.size(); ++r) {
    ImGui::TableNextRow();
    for (std::size_t c = 0; c < rows[r].size(); ++c) {
      ImGui::TableSetColumnIndex(static_cast<int>(c));
      ImGui::TextUnformatted(rows[r][c].c_str());
    }
  }
  ImGui::EndTable();
}

} // namespace

void ControlPanel::draw() {
  // Not `using namespace`: macOS headers define their own Boolean type
  namespace Controls = ScratchPadd::ControlType;
  for (const std::string &controlName : names_) {
    ControlTypeVariant &control = controls_.controlMap.at(controlName);
    const char *label = controlName.c_str();

    // Each widget edits the panel's copy and returns true when the user
    // changed it.
    const bool edited = std::visit(
        VariantHandler{
            [&](Controls::Double &value) {
              if (value.enforcableRange) {
                return ImGui::SliderScalar(label, ImGuiDataType_Double, &value.value,
                                           &value.enforcableRange->first,
                                           &value.enforcableRange->second);
              }
              return ImGui::InputDouble(label, &value.value);
            },
            [&](Controls::Integer &value) {
              if (value.enforcableRange) {
                return ImGui::SliderInt(label, &value.value, value.enforcableRange->first,
                                        value.enforcableRange->second);
              }
              return ImGui::InputInt(label, &value.value);
            },
            [&](Controls::Boolean &value) { return ImGui::Checkbox(label, &value.value); },
            [&](Controls::Info &value) {
              // Shown, not edited: as a table if it's lines of tab-separated
              // columns (the first line its header), otherwise as text
              if (value.value.find('\t') != std::string::npos)
                drawTable(label, value.value);
              else
                ImGui::LabelText(label, "%s", value.value.c_str());
              return false;
            },
            [&](Controls::String &value) {
              if (!value.enforcableRange) {
                return ImGui::InputText(label, &value.value); // grows as needed
              }
              bool changed = false;
              if (ImGui::BeginCombo(label, value.value.c_str())) {
                for (const std::string &option : *value.enforcableRange) {
                  const bool selected = option == value.value;
                  if (ImGui::Selectable(option.c_str(), selected) && !selected) {
                    value.value = option;
                    changed = true;
                  }
                  if (selected) {
                    ImGui::SetItemDefaultFocus();
                  }
                }
                ImGui::EndCombo();
              }
              return changed;
            },
        },
        control);

    if (edited && onChange_) {
      onChange_(
          MessageType::ControlChange{.controlName = controlName, .controlTypeValue = control});
    }
  }
}

ControlPanels::ControlPanels(Send send, ImVec2 placement)
    : send_(std::move(send)), placement_(placement) {}

void ControlPanels::show(const MessageType::ControlSnapshot &snapshot, PaddAddress owner) {
  auto existing = std::ranges::find_if(
      tabs_, [&](const Tab &tab) { return tab.paddName == snapshot.paddName; });
  if (snapshot.controlMap.empty()) {
    if (existing != tabs_.end())
      tabs_.erase(existing);
    return;
  }
  if (existing != tabs_.end()) {
    existing->panel->setControls(snapshot);
    return;
  }
  auto panel = std::make_unique<ControlPanel>();
  panel->setControls(snapshot);
  panel->setOnChange([this, owner, padd = snapshot.paddName](
                         const MessageType::ControlChange &change) { send_(owner, padd, change); });
  tabs_.push_back({snapshot.paddName, std::move(panel)});
}

void ControlPanels::draw() {
  if (tabs_.empty())
    return;
  // Kept a 10 px margin from the display's edge it's placed against
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(
      ImVec2(10 + placement_.x * (display.x - 20), 10 + placement_.y * (display.y - 20)),
      ImGuiCond_FirstUseEver, placement_);
  // Sized to fit the chosen tab's controls, so labels aren't cut off and
  // nothing scrolls
  const bool shown = Theme::beginWindow("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  if (shown && ImGui::BeginTabBar("padds", ImGuiTabBarFlags_Reorderable |
                                               ImGuiTabBarFlags_FittingPolicyScroll)) {
    for (Tab &tab : tabs_) {
      // (the label shown, the padd's whole name its ID)
      const std::string label = tabLabel(tab.paddName) + "###" + tab.paddName;
      if (ImGui::BeginTabItem(label.c_str())) {
        tab.panel->draw();
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

std::vector<std::string> ControlPanels::tabNames() const {
  std::vector<std::string> names;
  names.reserve(tabs_.size());
  for (const Tab &tab : tabs_)
    names.push_back(tab.paddName);
  return names;
}

std::string ControlPanels::tabLabel(const std::string &paddName) {
  if (paddName.size() > 4 && paddName.ends_with("Padd"))
    return paddName.substr(0, paddName.size() - 4);
  return paddName;
}

} // namespace ScratchPadd::DebugUi
