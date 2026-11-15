// DeviceProfilePadd in a running system: the profile published when it
// starts; a value changed on its tab (as a panel sends it) published, made
// current, and saved; and a reset back to the built-in profile.
#include <ScratchPadd/DeviceProfile/DeviceProfilePadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;
using ScratchPadd::DeviceProfile;

namespace {

std::string folder() { return ::testing::TempDir() + "/device_profile"; }

class TestProfilePadd : public ScratchPadd::DeviceProfilePadd {
public:
  using DeviceProfilePadd::DeviceProfilePadd;

protected:
  std::optional<ScratchPadd::Storage::Folders> foldersOverride() override {
    return ScratchPadd::Storage::Folders::under(folder());
  }
};

// Stands in for a panel and a padd reading the profile
class Reader : public ScratchPadd::Base {
public:
  Reader(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Reader"; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::Steadily; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  template <typename Job> auto run(Job job) {
    auto promise = std::make_shared<std::promise<decltype(job(*this))>>();
    push([this, promise, job] { promise->set_value(job(*this)); });
    auto future = promise->get_future();
    EXPECT_EQ(future.wait_for(5s), std::future_status::ready);
    return future.get();
  }
  ScratchPadd::State<DeviceProfile> profile() { return currentState<DeviceProfile>(); }
  void change(const std::string &control, ScratchPadd::ControlTypeVariant value) {
    sendTo(addressOf("DeviceProfilePadd"),
           ScratchPadd::MessageType::ControlChange{.controlName = control,
                                                   .controlTypeValue = std::move(value)});
  }
};

template <typename Done> bool eventually(Reader &reader, Done done) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (reader.run([&](Reader &r) { return done(r.profile()); }))
      return true;
    std::this_thread::sleep_for(10ms);
  }
  return false;
}

} // namespace

TEST(DeviceProfilePaddTest, PublishedAdjustedSavedAndReset) {
  std::filesystem::remove_all(folder());
  ScratchPadd::setCurrentProfile(DeviceProfile::forThisDevice());
  ScratchPadd::SystemImplementation<Reader, TestProfilePadd> system;
  system.instantiate();
  std::thread mainThread([&] { system.start(); });
  system.waitForStart();
  auto &reader = *std::get<std::unique_ptr<Reader>>(system.getWorkers());

  ASSERT_TRUE(
      eventually(reader, [](const ScratchPadd::State<DeviceProfile> &p) { return p.version > 0; }));
  EXPECT_EQ(reader.run([](Reader &r) { return r.profile().value; }),
            DeviceProfile::forThisDevice());

  if (ScratchPadd::DeviceProfilePadd::kTab) {
    reader.run([](Reader &r) {
      r.change("physics.subSteps", ScratchPadd::ControlType::Integer(2));
      return 0;
    });
    EXPECT_TRUE(eventually(reader, [](const ScratchPadd::State<DeviceProfile> &p) {
      return p.value.physics.subSteps == 2;
    }));
    EXPECT_EQ(ScratchPadd::currentProfile().physics.subSteps, 2);
    std::ifstream file(folder() + "/" + ScratchPadd::kProfileFile);
    std::stringstream text;
    text << file.rdbuf();
    EXPECT_NE(text.str().find("physics.subSteps 2"), std::string::npos) << text.str();

    reader.run([](Reader &r) {
      r.change("Reset to the built-in profile", ScratchPadd::ControlType::Boolean(true));
      return 0;
    });
    EXPECT_TRUE(eventually(reader, [](const ScratchPadd::State<DeviceProfile> &p) {
      return p.value == DeviceProfile::forThisDevice();
    }));
  }
  system.stop();
  mainThread.join();
}
