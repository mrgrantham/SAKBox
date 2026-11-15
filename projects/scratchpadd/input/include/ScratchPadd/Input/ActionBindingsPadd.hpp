// ActionBindingsPadd.hpp - remapping a game's actions (Actions.hpp) from the
// controls window: for the pad in hand, each action's two bindings as a
// choice from a list, named as that pad names its buttons ("A", "Cross",
// "Left stick across", "LB/RB"), and "Reset to defaults". Remaps are kept
// per kind of pad, saved in the app's files (controls.txt, by StoragePadd
// when the app has one) and read back at start, and published as shared
// state for the game's ActionReader:
//
//   using ShipBindings = Input::ActionBindingsPadd<Ship, 4, kShipActions, shipDefaults>;
//   // listed in the system with GamePadd; in the game's padd, once a frame:
//   controls.use(currentState<Input::SharedBindings<Ship, 4>>().value);
//
// The choices are the controls every panel already shows (a string with a
// list), so remapping works in the desktop window, on a console's own
// panel and from the remote controls app alike.
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Input/Actions.hpp>
#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>
#include <ScratchPadd/Storage/Messages.hpp>

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ScratchPadd::Input {

template <typename Action, std::size_t N, const ActionSet<Action, N> &kSet,
          Bindings<N> (*kDefaults)(Kind)>
class ActionBindingsPadd : public Base {
public:
  static constexpr const char *kFile = "controls.txt";

  explicit ActionBindingsPadd(System *system) : Base(system) {}
  std::string name() override { return "ActionBindingsPadd"; }
  ControlValueMap generateControls() override { return {}; }
  // Checks which pad is in hand a few times a second
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::milliseconds(250);
  }

  void prepare() override {
    if (const auto bytes = Storage::loadFile(filesFolder() + kFile)) {
      const std::string text(bytes->begin(), bytes->end());
      for (const auto &[kind, lines] : remapsFromText(text))
        remaps_[kind] = bindingsFromText(kSet, lines, kDefaults(kind));
    }
  }

  void starting() override { repeat(); }

  void repeat() override {
    const Gamepad pad = currentState<Gamepad>().value;
    if (shown_ && pad.kind == kind_)
      return;
    kind_ = pad.kind;
    pad_ = pad;
    shown_ = true;
    showControls();
    publish();
  }

  void controlChanged(const std::string &name, const ControlTypeVariant &) override {
    if (name == kReset) {
      remaps_.erase(kind_);
    } else {
      const auto slot = slotNamed(name);
      const auto *chosen = std::get_if<ControlType::String>(&controlValue(name));
      if (!slot || !chosen)
        return;
      Bindings<N> bindings = current();
      const auto choices = bindingChoices(kSet.kind(slot->first), pad_);
      for (const auto &[label, binding] : choices) {
        if (label == chosen->value)
          bindings.slots[ActionSet<Action, N>::indexOf(slot->first)][slot->second] = binding;
      }
      remaps_[kind_] = bindings;
    }
    showControls();
    publish();
    save();
  }

protected:
  // Somewhere else for controls.txt than the app's files folder (tests);
  // then it's read and written here, not by StoragePadd
  virtual std::optional<Storage::Folders> foldersOverride() { return std::nullopt; }

private:
  static constexpr const char *kReset = "Reset to defaults";

  std::string filesFolder() { return foldersOverride().value_or(Storage::Folders::forApp()).files; }

  Bindings<N> current() const {
    const auto found = remaps_.find(kind_);
    return found != remaps_.end() ? found->second : kDefaults(kind_);
  }

  static std::string slotName(const ActionInfo<Action> &info, std::size_t slot) {
    return std::string(info.name) + " " + std::to_string(slot + 1);
  }
  std::optional<std::pair<Action, std::size_t>> slotNamed(const std::string &name) const {
    for (const ActionInfo<Action> &info : kSet.actions) {
      for (std::size_t slot = 0; slot < kBindingsPerAction; ++slot) {
        if (slotName(info, slot) == name)
          return std::pair{info.action, slot};
      }
    }
    return std::nullopt;
  }

  // A choice for each action's two slots, showing what's bound now
  void showControls() {
    const Bindings<N> bindings = current();
    for (const ActionInfo<Action> &info : kSet.actions) {
      const auto choices = bindingChoices(info.kind, pad_);
      std::vector<std::string> labels;
      labels.reserve(choices.size());
      for (const auto &[label, binding] : choices)
        labels.push_back(label);
      for (std::size_t slot = 0; slot < kBindingsPerAction; ++slot) {
        const Binding &bound = bindings.of(info.action)[slot];
        std::string shown = labels.front(); // "none"
        for (const auto &[label, binding] : choices) {
          if (binding == bound)
            shown = label;
        }
        replaceControl(slotName(info, slot), ControlType::String(shown, labels));
      }
    }
    replaceControl(kReset, ControlType::Boolean(false));
    replaceControl("Pad",
                   ControlType::Info(std::string(profileOf(kind_).name) +
                                     (remaps_.contains(kind_) ? " (remapped)" : " (defaults)")));
  }

  void replaceControl(const std::string &name, ControlTypeVariant control) {
    if (hasControl(name))
      removeControl(name);
    addControl(name, std::move(control));
  }

  void publish() {
    publishState(SharedBindings<Action, N>{kind_, remaps_.contains(kind_), current()});
  }

  // Into the app's files: by StoragePadd if the app has one (on its thread,
  // when nothing else has work), or here
  void save() {
    std::map<Kind, std::string> perKind;
    for (const auto &[kind, bindings] : remaps_)
      perKind[kind] = bindingsText(kSet, bindings);
    const std::string text = remapsText(perKind);
    auto bytes = std::make_shared<const Storage::Bytes>(text.begin(), text.end());
    const PaddAddress storage = foldersOverride() ? PaddAddress() : addressOf("StoragePadd");
    if (storage) {
      sendTo(storage, Storage::SaveFile{.path = kFile, .bytes = std::move(bytes)});
    } else {
      std::string error;
      if (!Storage::saveFile(filesFolder() + kFile, *bytes, error))
        logger().warn("{}: couldn't save {}: {}", name(), kFile, error);
    }
  }

  std::map<Kind, Bindings<N>> remaps_;
  Kind kind_ = Kind::None;
  Gamepad pad_;
  bool shown_ = false;
};

} // namespace ScratchPadd::Input
