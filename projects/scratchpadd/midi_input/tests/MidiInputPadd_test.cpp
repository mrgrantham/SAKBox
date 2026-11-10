// MidiInputPadd with a virtual MIDI port standing in for a keyboard: what's
// played on it reaches the padds that listen, and AudioPadd's instrument.
// On Windows, whose MIDI has no virtual ports for a program to open, the
// keyboard is a loopback pair made with Windows MIDI Services' console.
// Skipped where there's no MIDI system (a Linux container without ALSA's
// sequencer, say) or, on Windows, no Windows MIDI Services tools.
#include <AudioPadd.hpp>
#include <ExpectedCapabilities/ExpectedCapabilities.hpp>
#include <MidiInputPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using ScratchPadd::Audio::MidiEvent;
using ScratchPadd::MidiInput::MidiInput;

namespace {

// This test program's process id
int processId() {
#if defined(_WIN32)
  return _getpid();
#else
  return getpid();
#endif
}

// Each test's keyboard has its own name, so a test can't find the last
// one's as it goes away, nor another test program's (MIDI ports are shared
// by every program on the machine, and Bazel runs tests side by side). It's
// kept short: Windows' MIDI 1.0 API cuts port names off at 31 characters
// (ALSA at 63), and a cut-off name wouldn't match. The test's line in this
// file tells the tests apart.
std::string testTag() {
  return std::to_string(processId()) + " line " +
         std::to_string(testing::UnitTest::GetInstance()->current_test_info()->line());
}
std::string keyboardName() { return "SAKBox key " + testTag(); }

template <typename Condition> bool eventually(Condition condition) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > deadline)
      return false;
    std::this_thread::sleep_for(5ms);
  }
  return true;
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

// A MIDI source named keyboardName(), as a keyboard would be; unplugged
// when destroyed
class Keyboard {
public:
  virtual ~Keyboard() = default;
  void play(const MidiEvent &event) {
    const std::vector<unsigned char> bytes = {event.status, event.data1, event.data2};
    out_.sendMessage(&bytes);
  }

protected:
  RtMidiOut out_;
};

#if defined(_WIN32)

// Windows MIDI Services' console, from its tools
std::filesystem::path midiConsole() {
  const char *programFiles = std::getenv("ProgramFiles");
  return std::filesystem::path(programFiles ? programFiles : R"(C:\Program Files)") /
         "Windows MIDI Services" / "Tools" / "midi.exe";
}

// Runs COMMAND LINE with no window, and returns what it printed, or nothing
// if it couldn't run or failed
std::optional<std::string> runQuietly(std::string commandLine) {
  SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE read = nullptr, write = nullptr;
  if (!CreatePipe(&read, &write, &inheritable, 0))
    return std::nullopt;
  SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOA startup{.cb = sizeof(STARTUPINFOA)};
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = startup.hStdError = write;
  PROCESS_INFORMATION process{};
  const bool started = CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
  CloseHandle(write); // (so reading ends when the program does)
  std::string output;
  char buffer[4096];
  DWORD got = 0;
  while (started && ReadFile(read, buffer, sizeof buffer, &got, nullptr) && got > 0)
    output.append(buffer, got);
  CloseHandle(read);
  if (!started)
    return std::nullopt;
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exitCode = 1;
  GetExitCodeProcess(process.hProcess, &exitCode);
  CloseHandle(process.hProcess);
  CloseHandle(process.hThread);
  if (exitCode != 0)
    return std::nullopt;
  return output;
}

// A loopback pair: what's sent to its A side comes out of its B side,
// named keyboardName(). (A's name leaves that out, so the padd listens to
// B alone.) It lasts until removed, or the MIDI service restarts.
class LoopbackKeyboard : public Keyboard {
public:
  static std::unique_ptr<Keyboard> plugIn() {
    const std::string sender = "SAKBox out " + testTag();
    std::string id = "sakbox " + testTag(); // (letters and digits only)
    std::erase(id, ' ');
    const auto created =
        runQuietly(console() + " loopback create --name-a \"" + sender + "\" --name-b \"" +
                   keyboardName() + "\" --unique-identifier " + id);
    std::smatch association;
    if (!created || !std::regex_search(*created, association,
                                       std::regex(R"(Association Id\s+(\{[0-9A-Fa-f-]+\}))")))
      return nullptr;
    auto keyboard = std::unique_ptr<LoopbackKeyboard>(new LoopbackKeyboard(association[1]));
    // Its A side, once this program's MIDI sees it
    if (!eventually([&] { return keyboard->openPortNamed(sender); }))
      return nullptr;
    return keyboard;
  }
  ~LoopbackKeyboard() override {
    out_.closePort();
    runQuietly(console() + " loopback remove --association-id " + associationId_);
  }

private:
  explicit LoopbackKeyboard(std::string associationId) : associationId_(std::move(associationId)) {}
  static std::string console() { return "\"" + midiConsole().string() + "\""; }
  bool openPortNamed(const std::string &name) {
    for (unsigned int i = 0; i < out_.getPortCount(); ++i) {
      if (out_.getPortName(i).find(name) != std::string::npos) {
        out_.openPort(i);
        return out_.isPortOpen();
      }
    }
    return false;
  }

  std::string associationId_;
};

#else

// A virtual port, which Core MIDI and ALSA let a program open. (It throws if
// it can't; isPortOpen() is no sign, as RtMidi never marks a virtual port
// open.)
class VirtualKeyboard : public Keyboard {
public:
  static std::unique_ptr<Keyboard> plugIn() {
    auto keyboard = std::make_unique<VirtualKeyboard>();
    keyboard->out_.openVirtualPort(keyboardName());
    return keyboard;
  }
};

#endif

// The test's keyboard; nothing if this machine has no MIDI system, or on
// Windows no Windows MIDI Services tools
std::unique_ptr<Keyboard> plugInKeyboard() {
  try {
#if defined(_WIN32)
    if (!std::filesystem::exists(midiConsole()))
      return nullptr;
    return LoopbackKeyboard::plugIn();
#else
    return VirtualKeyboard::plugIn();
#endif
  } catch (const RtMidiError &) {
    return nullptr;
  }
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
  auto keyboard = plugInKeyboard();
  if (!keyboard) {
    SKIP_OR_FAIL_WITHOUT("midi", "no MIDI keyboard to make here");
  }
  Running running;
  ASSERT_TRUE(eventually([&] { return listening(running.padd<MidiInputPadd>()); }));
  keyboard->play(MidiEvent::noteOn(0, 60, 110));
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
  auto keyboard = plugInKeyboard();
  if (!keyboard) {
    SKIP_OR_FAIL_WITHOUT("midi", "no MIDI keyboard to make here");
  }
  Running running;
  auto &input = running.padd<MidiInputPadd>();
  ASSERT_TRUE(eventually([&] { return listening(input); }));
  ScratchPadd::onPaddThread(input, [](MidiInputPadd &p) {
    ScratchPadd::PaddTestAccess::setControl(p, "playThrough",
                                            ScratchPadd::ControlType::Boolean(false));
  });
  keyboard->play(MidiEvent::noteOn(0, 64, 110));
  ASSERT_TRUE(eventually([&] { return !running.padd<ListenerPadd>().heard().empty(); }));
  std::this_thread::sleep_for(200ms); // time enough for AudioPadd to have played it, if asked
  EXPECT_EQ(running.loudest(4096), 0.0f);
  keyboard.reset(); // unplugged
  EXPECT_TRUE(eventually([&] { return !listening(input); }));
}
