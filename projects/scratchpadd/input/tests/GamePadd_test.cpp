// GamePadd in a running system: it publishes the pad as shared state, with
// the keyboard a window's padd publishes laid on.
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;
namespace Input = ScratchPadd::Input;

namespace {

// Stands in for the window's padd (and a game reading the pad): publishes
// the keyboard, reads the pad, on its own thread
class WindowPadd : public ScratchPadd::Base {
public:
  WindowPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "WindowPadd"; }
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::BackgroundThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  template <typename Job> auto run(Job job) {
    auto promise = std::make_shared<std::promise<decltype(job(*this))>>();
    push([this, promise, job] { promise->set_value(job(*this)); });
    auto future = promise->get_future();
    EXPECT_EQ(future.wait_for(5s), std::future_status::ready);
    return future.get();
  }
  void publishKeys(const Input::Keyboard &keys) { publishState(keys); }
  ScratchPadd::State<Input::Gamepad> pad() { return currentState<Input::Gamepad>(); }
};

} // namespace

TEST(GamePaddTest, PublishesThePadWithTheKeyboard) {
  ScratchPadd::SystemImplementation<Input::GamePadd, WindowPadd> system;
  system.instantiate();
  std::thread mainThread([&] { system.start(); });
  system.waitForStart();
  auto &window = *std::get<std::unique_ptr<WindowPadd>>(system.getWorkers());

  // Published from the start (the platform's pad, if one's connected)
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (window.run([](WindowPadd &w) { return w.pad().version; }) == 0 &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(5ms);

  Input::Keyboard keys;
  keys.open = true;
  keys.set(Input::Key::K, true);
  window.run([&](WindowPadd &w) {
    w.publishKeys(keys);
    return 0;
  });
  bool held = false;
  while (!held && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(5ms);
    held =
        window.run([](WindowPadd &w) { return w.pad().value.isHeld(Input::Button::FaceBottom); });
  }
  EXPECT_TRUE(held);
  const auto pad = window.run([](WindowPadd &w) { return w.pad(); });
  EXPECT_NE(pad.value.kind, Input::Kind::None);
  EXPECT_GT(pad.version, 1u); // read again and again

  system.stop();
  mainThread.join();
}
