
#include <ScratchPadd/Graphics/Graphics.hpp>

void Graphics::ControlView::setControls(
    ScratchPadd::MessageType::Control &controls) {
  controls_ = controls;
  for (auto &&[key, value] : controls_.controlMap) {
    orderedControlVector_.push_back(key);
  }
  std::sort(orderedControlVector_.begin(), orderedControlVector_.end());
}

void Graphics::ControlView::buildControls(
    ScratchPadd::MessageType::Control &controls) {
  for (auto &&controlName : orderedControlVector_) {
    ScratchPadd::ControlTypeVariant &controlTypeVariant =
        controls.controlMap[controlName];
    std::visit(VariantHandler{
                   [&](ScratchPadd::ControlType::Double &control) {
                     if (control.enforcableRange) {
                       ImGui::InputDouble(controlName.c_str(), &control.value);
                     } else {
                     }
                   },
                   [&](ScratchPadd::ControlType::Integer &control) {
                     if (control.enforcableRange) {
                       ImGui::InputInt(controlName.c_str(), &control.value);
                     } else {
                     }
                   },
                   [&](ScratchPadd::ControlType::Boolean &control) {
                     ImGui::Checkbox(controlName.c_str(), &control.value);
                   },
                   [&](ScratchPadd::ControlType::String &control) {
                     ImGui::InputText(controlName.c_str(),
                                      (char *)control.value.c_str(),
                                      control.value.capacity() + 1);
                   }},
               controlTypeVariant);
  }
}