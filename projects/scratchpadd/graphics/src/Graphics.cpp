#include <ScratchPadd/Graphics/Graphics.hpp>
#include <ScratchPadd/Helper.hpp>

#include <algorithm>
#include <cstdint>
#include <imgui_stdlib.h>
#include <type_traits>

static_assert(std::is_same_v<int32_t, int>, "ImGui's integer widgets take int*");

void Graphics::ControlView::setControls(const ScratchPadd::MessageType::ControlSnapshot &controls) {
  controls_ = controls;
  orderedControlVector_.clear();
  for (const auto &[name, value] : controls_.controlMap) {
    orderedControlVector_.push_back(name);
  }
  std::sort(orderedControlVector_.begin(), orderedControlVector_.end());
}

void Graphics::ControlView::buildControls() {
  // Not `using namespace`: macOS headers define their own Boolean type
  namespace Controls = ScratchPadd::ControlType;
  for (const std::string &controlName : orderedControlVector_) {
    ScratchPadd::ControlTypeVariant &control = controls_.controlMap.at(controlName);
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
      onChange_(ScratchPadd::MessageType::ControlChange{
          .paddName = controls_.paddName, .controlName = controlName, .controlTypeValue = control});
    }
  }
}
