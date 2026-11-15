// ActionBindingsPadd in a running system: the bindings for the pad in hand
// published (the defaults at first); a choice made in a panel (a
// ControlChange) remapping an action, published at once and saved to
// controls.txt; and a new system reading the remap back.
#include <ScratchPadd/Input/ActionBindingsPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;
namespace Input = ScratchPadd::Input;

namespace {

enum class Ship { Steer, Confirm };
constexpr Input::ActionSet<Ship, 2> kShip{{{
    {Ship::Steer, "steer", Input::ActionKind::Axis},
    {Ship::Confirm, "confirm", Input::ActionKind::Button},
}}};

Input::Bindings<2> shipDefaults(Input::Kind) {
  Input::Bindings<2> b;
  b.bind(Ship::Steer, Input::Binding::stick(Input::Side::Left, Input::Direction::X));
  b.bind(Ship::Confirm, Input::Binding::of(Input::Button::FaceBottom));
  return b;
}

std::string folder() { return ::testing::TempDir() + "/action_bindings"; }

// The padd under test, keeping controls.txt in the test's folder
class TestBindingsPadd : public Input::ActionBindingsPadd<Ship, 2, kShip, shipDefaults> {
public:
  using ActionBindingsPadd::ActionBindingsPadd;

protected:
  std::optional<ScratchPadd::Storage::Folders> foldersOverride() override {
    return ScratchPadd::Storage::Folders::under(folder());
  }
};

// Stands in for GamePadd (publishing an Xbox pad) and for a panel and the
// game: sends the padd a choice, and reads the bindings it publishes
class PadAndPanel : public ScratchPadd::Base {
public:
  PadAndPanel(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "PadAndPanel"; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::Steadily; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void starting() override {
    Input::Gamepad pad;
    pad.kind = Input::Kind::Xbox;
    pad.features = Input::profileOf(Input::Kind::Xbox).features;
    publishState(pad);
  }

  template <typename Job> auto run(Job job) {
    auto promise = std::make_shared<std::promise<decltype(job(*this))>>();
    push([this, promise, job] { promise->set_value(job(*this)); });
    auto future = promise->get_future();
    EXPECT_EQ(future.wait_for(5s), std::future_status::ready);
    return future.get();
  }
  Input::SharedBindings<Ship, 2> bindings() {
    return currentState<Input::SharedBindings<Ship, 2>>().value;
  }
  void choose(const std::string &control, const std::string &choice) {
    sendTo(addressOf("ActionBindingsPadd"),
           ScratchPadd::MessageType::ControlChange{.controlName = control,
                                                   .controlTypeValue =
                                                       ScratchPadd::ControlType::String(choice)});
  }
};

using TestSystem = ScratchPadd::SystemImplementation<PadAndPanel, TestBindingsPadd>;

// Waits for the published bindings to satisfy `done`
template <typename Done> bool eventually(PadAndPanel &panel, Done done) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (panel.run([&](PadAndPanel &p) { return done(p.bindings()); }))
      return true;
    std::this_thread::sleep_for(10ms);
  }
  return false;
}

} // namespace

TEST(ActionBindingsPaddTest, RemapsArePublishedSavedAndReadBack) {
  std::filesystem::remove_all(folder());
  {
    TestSystem system;
    system.instantiate();
    std::thread mainThread([&] { system.start(); }); // (start() runs until stop())
    system.waitForStart();
    auto &panel = *std::get<std::unique_ptr<PadAndPanel>>(system.getWorkers());

    // The defaults, for the Xbox pad in hand
    ASSERT_TRUE(eventually(panel, [](const Input::SharedBindings<Ship, 2> &b) {
      return b.kind == Input::Kind::Xbox && !b.remapped;
    }));
    EXPECT_EQ(panel.run([](PadAndPanel &p) { return p.bindings().bindings; }),
              shipDefaults(Input::Kind::Xbox));

    // Confirm moved to the right face button ("B" on an Xbox pad)
    const std::string b = Input::profileOf(Input::Kind::Xbox)
                              .labels[static_cast<std::size_t>(Input::Button::FaceRight)];
    panel.run([&](PadAndPanel &p) {
      p.choose("confirm 1", b);
      return 0;
    });
    EXPECT_TRUE(eventually(panel, [](const Input::SharedBindings<Ship, 2> &shared) {
      return shared.remapped &&
             shared.bindings.of(Ship::Confirm)[0] == Input::Binding::of(Input::Button::FaceRight);
    }));
    system.stop();
    mainThread.join();
  }
  std::ifstream file(folder() + "/controls.txt"); // (Folders::under: files in the folder itself)
  std::stringstream text;
  text << file.rdbuf();
  EXPECT_NE(text.str().find("[xbox]"), std::string::npos) << text.str();
  EXPECT_NE(text.str().find("confirm button:face_right"), std::string::npos) << text.str();

  // A new run starts with the remap
  TestSystem again;
  again.instantiate();
  std::thread againThread([&] { again.start(); });
  again.waitForStart();
  auto &panel = *std::get<std::unique_ptr<PadAndPanel>>(again.getWorkers());
  EXPECT_TRUE(eventually(panel, [](const Input::SharedBindings<Ship, 2> &shared) {
    return shared.remapped &&
           shared.bindings.of(Ship::Confirm)[0] == Input::Binding::of(Input::Button::FaceRight);
  }));
  again.stop();
  againThread.join();
}

// The choices name the pad's own buttons, and offer only what it has
TEST(ActionBindingsPaddTest, ChoicesFollowThePad) {
  Input::Gamepad ds;
  ds.kind = Input::Kind::Ds;
  ds.features = Input::profileOf(Input::Kind::Ds).features;
  const auto axisChoices = Input::bindingChoices(Input::ActionKind::Axis, ds);
  for (const auto &[label, binding] : axisChoices)
    EXPECT_EQ(label.find("Right stick"), std::string::npos) << label; // a DS has none
  EXPECT_EQ(axisChoices.front().first, "none");
  Input::Gamepad xbox;
  xbox.kind = Input::Kind::Xbox;
  xbox.features = Input::profileOf(Input::Kind::Xbox).features;
  bool rightStick = false;
  for (const auto &[label, binding] : Input::bindingChoices(Input::ActionKind::Axis, xbox))
    rightStick = rightStick || label == "Right stick across";
  EXPECT_TRUE(rightStick);
  // The settings file: a section per kind of pad
  const std::map<Input::Kind, std::string> remaps{
      {Input::Kind::Xbox, "confirm button:face_right\n"},
      {Input::Kind::Ds, "steer pair:left:right\n"}};
  EXPECT_EQ(Input::remapsFromText(Input::remapsText(remaps)), remaps);
}
