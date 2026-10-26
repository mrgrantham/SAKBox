// MidiInputPadd with a virtual MIDI port standing in for a keyboard: what's
// played on it reaches the padds that listen, and AudioPadd's instrument.
// Skipped where there's no MIDI system (a Linux container without ALSA's
// sequencer, say).
#include <AudioPadd.hpp>
#include <MidiInputPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <unistd.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using ScratchPadd::Audio::MidiEvent;
using ScratchPadd::MidiInput::MidiInput;

namespace {

// Each test's keyboard has its own name, so a test can't find the last
// one's as it goes away, nor another test program's (MIDI ports are shared
// by every program on the machine, and Bazel runs tests side by side)
std::string keyboardName() {
  return "SAKBox test keyboard " + std::to_string(getpid()) + " " +
         testing::UnitTest::GetInstance()->current_test_info()->name();
}

// Keeps the MidiInput messages it hears
class ListenerPadd : public ScratchPadd::Base {
public:
  ListenerPadd(ScratchPadd::System *system) : Base(system) {
    receive<MidiInput>([this](const MidiInput &input) {
      std::lock_guard<std::mutex> lk(mutex_);
      heard_.push_back(input);
    });
  }
  std::string name() override { return "Listener"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::vector<MidiInput> heard() {
    std::lock_guard<std::mutex> lk(mutex_);
    return heard_;
  }

private:
  std::mutex mutex_;
  std::vector<MidiInput> heard_;
};

template <typename Condition> bool eventually(Condition condition) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > deadline)
      return false;
    std::this_thread::sleep_for(5ms);
  }
  return true;
}

// A virtual MIDI source, as a keyboard would be; nothing if this machine
// has no MIDI system
std::unique_ptr<RtMidiOut> virtualKeyboard() {
  try {
    auto keyboard = std::make_unique<RtMidiOut>();
    keyboard->openVirtualPort(keyboardName());
    return keyboard;
  } catch (const RtMidiError &) {
    return nullptr;
  }
}

void play(RtMidiOut &keyboard, const MidiEvent &event) {
  const std::vector<unsigned char> bytes = {event.status, event.data1, event.data2};
  keyboard.sendMessage(&bytes);
}

// The system, with AudioPadd mixing offline for the test to hear
struct Running {
  ScratchPadd::SystemImplementation<MidiInputPadd, ListenerPadd, AudioPadd> system;
  ScratchPadd::Audio::OfflineOutput *output = nullptr;
  std::thread mainThread;
  Running() {
    // Only this test's keyboard: not a real one, nor another test's
    MidiInputPadd::portFilter() = [name = keyboardName()](const std::string &port) {
      return port.find(name) != std::string::npos;
    };
    AudioPadd::outputFactory() = [this] {
      auto offline = std::make_unique<ScratchPadd::Audio::OfflineOutput>();
      output = offline.get();
      return offline;
    };
    system.instantiate();
    mainThread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~Running() {
    system.stop();
    mainThread.join();
    AudioPadd::outputFactory() = [] {
      return std::make_unique<ScratchPadd::Audio::DeviceOutput>();
    };
    MidiInputPadd::portFilter() = [](const std::string &) { return true; };
  }
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
  float loudest(std::uint32_t frames) {
    std::vector<float> out(2 * frames);
    output->render(out.data(), frames);
    float peak = 0;
    for (float sample : out)
      peak = std::max(peak, std::abs(sample));
    return peak;
  }
};

// Whether the padd lists the keyboard among its ports yet
bool listening(MidiInputPadd &padd) {
  return ScratchPadd::onPaddThread(padd, [](MidiInputPadd &p) {
           return ScratchPadd::PaddTestAccess::control<ScratchPadd::ControlType::String>(p, "ports")
               .value;
         }).find(keyboardName()) != std::string::npos;
}

} // namespace

// A note played on the keyboard reaches listeners, with its port, and plays
// on AudioPadd's instrument
TEST(MidiInputPaddTest, KeyboardReachesListenersAndTheInstrument) {
  auto keyboard = virtualKeyboard();
  if (!keyboard) {
    GTEST_SKIP() << "no MIDI system here";
  }
  Running running;
  ASSERT_TRUE(eventually([&] { return listening(running.padd<MidiInputPadd>()); }));
  play(*keyboard, MidiEvent::noteOn(0, 60, 110));
  ASSERT_TRUE(eventually([&] { return !running.padd<ListenerPadd>().heard().empty(); }));
  const MidiInput heard = running.padd<ListenerPadd>().heard()[0];
  EXPECT_EQ(heard.event.status, 0x90);
  EXPECT_EQ(heard.event.data1, 60);
  EXPECT_EQ(heard.event.data2, 110);
  EXPECT_NE(heard.port.find(keyboardName()), std::string::npos) << heard.port;
  EXPECT_TRUE(eventually([&] { return running.loudest(1024) > 0.01f; }));
}

// With playThrough off, listeners still hear the keyboard but AudioPadd
// doesn't play it; a keyboard unplugged is let go
TEST(MidiInputPaddTest, PlayThroughOffAndUnplugging) {
  auto keyboard = virtualKeyboard();
  if (!keyboard) {
    GTEST_SKIP() << "no MIDI system here";
  }
  Running running;
  auto &input = running.padd<MidiInputPadd>();
  ASSERT_TRUE(eventually([&] { return listening(input); }));
  ScratchPadd::onPaddThread(input, [](MidiInputPadd &p) {
    ScratchPadd::PaddTestAccess::setControl(p, "playThrough",
                                            ScratchPadd::ControlType::Boolean(false));
  });
  play(*keyboard, MidiEvent::noteOn(0, 64, 110));
  ASSERT_TRUE(eventually([&] { return !running.padd<ListenerPadd>().heard().empty(); }));
  std::this_thread::sleep_for(200ms); // time enough for AudioPadd to have played it, if asked
  EXPECT_EQ(running.loudest(4096), 0.0f);
  keyboard.reset(); // unplugged
  EXPECT_TRUE(eventually([&] { return !listening(input); }));
}
