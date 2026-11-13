// GamePadd.hpp - the controller as shared state: a padd on the main thread
// that reads the pad (Reader.hpp) and publishes it, with a desktop's
// keyboard laid on (the window's padd publishes an Input::Keyboard; see
// Scene::Display::keyboard()). Any padd reads it:
//
//   const auto [pad, version, published] = currentState<Input::Gamepad>();
//
// It reads every 4 ms, but the main thread is busy drawing until the
// screen's refresh, so in practice once a frame, just before the next one.
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Input/Reader.hpp>

#include <chrono>
#include <string>

namespace ScratchPadd::Input {

class GamePadd : public Base {
public:
  explicit GamePadd(System *system) : Base(system) {}
  std::string name() override { return "GamePadd"; }
  RunOn runOn() override { return RunOn::MainThread; }
  ControlValueMap generateControls() override { return {}; }
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::milliseconds(4);
  }
  void starting() override { read(); }
  void repeat() override { read(); }

protected:
  // The reader, to change its key map or dead zone in a padd made from this
  Reader &reader() { return reader_; }

private:
  void read() {
    const State<Keyboard> keyboard = currentState<Keyboard>();
    publishState(reader_.read(keyboard.version ? &keyboard.value : nullptr));
  }
  Reader reader_;
};

} // namespace ScratchPadd::Input
