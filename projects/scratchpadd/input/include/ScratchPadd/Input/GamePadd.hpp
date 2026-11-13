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

#include <algorithm>
#include <chrono>
#include <string>

namespace ScratchPadd::Input {

class GamePadd : public Base {
public:
  explicit GamePadd(System *system) : Base(system) {}
  std::string name() override { return "GamePadd"; }
  Runs runs() override { return Runs::OnMainThread; }
  ControlValueMap generateControls() override { return {}; }
  std::optional<std::chrono::duration<double>> repeatInterval() override { return kReadEvery; }
  void starting() override { read(); }
  void repeat() override { read(); }

protected:
  // The reader, to change its key map or dead zone in a padd made from this
  Reader &reader() { return reader_; }

private:
  static constexpr auto kReadEvery = std::chrono::milliseconds(4);
  using Clock = std::chrono::steady_clock;
  // Reads further apart than this are logged (at most once per kLateReport):
  // three frames, when the main thread was kept from reading by other work
  static constexpr auto kLateGap = std::chrono::milliseconds(50);
  static constexpr auto kLateReport = std::chrono::seconds(10);

  void read() {
    noteGap();
    const State<Keyboard> keyboard = currentState<Keyboard>();
    publishState(reader_.read(keyboard.version ? &keyboard.value : nullptr));
  }
  // The longest time between two reads, logged when it's long (a button
  // pressed and let go in such a gap would be missed where the platform
  // doesn't count presses itself)
  void noteGap() {
    const Clock::time_point now = Clock::now();
    if (lastRead_ != Clock::time_point{}) {
      longestGap_ = std::max(longestGap_, now - lastRead_);
    }
    lastRead_ = now;
    if (longestGap_ > kLateGap && now - lastReport_ > kLateReport) {
      logger().warn("Reads were up to {:.0f} ms apart (asked for every {:.0f} ms)",
                    std::chrono::duration<double, std::milli>(longestGap_).count(),
                    std::chrono::duration<double, std::milli>(kReadEvery).count());
      longestGap_ = {};
      lastReport_ = now;
    }
  }

  Reader reader_;
  Clock::time_point lastRead_{}, lastReport_{};
  Clock::duration longestGap_{};
};

} // namespace ScratchPadd::Input
