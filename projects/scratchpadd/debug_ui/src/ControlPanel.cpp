#include <ScratchPadd/DebugUi/ControlPanel.hpp>
#include <ScratchPadd/DebugUi/Theme.hpp>
#include <ScratchPadd/Helper.hpp>

#include <algorithm>
#include <cstdint>
#include <imgui_stdlib.h>
#include <type_traits>

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

void ControlPanel::draw() {
  if (placement_) {
    // Keep a 10 px margin from the window edge the panel is placed against
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 position(10 + placement_->x * (display.x - 20),
                          10 + placement_->y * (display.y - 20));
    ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver, *placement_);
  }
  // Sized to fit its controls, so labels aren't cut off and nothing scrolls
  Theme::beginWindow(controls_.paddName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

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
  ImGui::End();
}

ControlPanels::ControlPanels(Send send, Placement placement)
    : send_(std::move(send)), placement_(std::move(placement)) {}

void ControlPanels::show(const MessageType::ControlSnapshot &snapshot, PaddAddress owner) {
  auto existing = panels_.find(snapshot.paddName);
  if (snapshot.controlMap.empty()) {
    if (existing != panels_.end())
      panels_.erase(existing);
    return;
  }
  if (existing != panels_.end()) {
    existing->second->setControls(snapshot);
    return;
  }
  auto panel = std::make_unique<ControlPanel>();
  panel->setControls(snapshot);
  panel->setOnChange(
      [this, owner](const MessageType::ControlChange &change) { send_(owner, change); });
  panel->setInitialPlacement(placement_(snapshot.paddName, opened_++));
  panels_[snapshot.paddName] = std::move(panel);
}

void ControlPanels::draw() {
  for (auto &[name, panel] : panels_)
    panel->draw();
}

ImVec2 ControlPanels::alongTheBottom(const std::string &, std::size_t index) {
  constexpr ImVec2 kSpots[] = {{0.0f, 1.0f}, {0.5f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};
  return kSpots[index % std::size(kSpots)];
}

} // namespace ScratchPadd::DebugUi
