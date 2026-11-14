// StoragePadd in a running system: a registered source's pieces written on
// its thread when the source rings, sources registered while it runs
// attached (and their controls shown) and let go when removed, whole files
// saved for a padd and answered, and the sources writing for themselves
// again once it stops.
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Storage/AppendFile.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;
namespace Storage = ScratchPadd::Storage;

namespace {

std::string root() { return ::testing::TempDir() + "/storage"; }

std::string contentsOf(const std::string &path) {
  std::ifstream file(path);
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

// Waits up to 2 s for `done`
template <typename Done> bool eventually(Done done) {
  for (auto until = std::chrono::steady_clock::now() + 2s; std::chrono::steady_clock::now() < until;
       std::this_thread::sleep_for(5ms))
    if (done())
      return true;
  return done();
}

// StoragePadd with the app's files under the test's own folder
class TestStoragePadd : public Storage::StoragePadd {
public:
  explicit TestStoragePadd(ScratchPadd::System *system)
      : StoragePadd(system, Storage::Folders::under(root())) {}
  bool has(const std::string &control) {
    return ScratchPadd::onPaddThread(*this,
                                     [&](TestStoragePadd &p) { return p.hasControl(control); });
  }
};

// Asks StoragePadd to save files and keeps its answers
class SaverPadd : public ScratchPadd::Base {
public:
  explicit SaverPadd(ScratchPadd::System *system) : Base(system) {
    receive<Storage::FileSaved>([this](const Storage::FileSaved &saved) {
      std::lock_guard lock(mutex_);
      answers_.push_back(saved);
    });
  }
  std::string name() override { return "Saver"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void save(const std::string &path, const std::string &text) {
    sendTo(addressOf("StoragePadd"),
           Storage::SaveFile{.path = path,
                             .bytes =
                                 std::make_shared<const Storage::Bytes>(text.begin(), text.end())});
  }
  std::vector<Storage::FileSaved> answers() {
    std::lock_guard lock(mutex_);
    return answers_;
  }

private:
  std::mutex mutex_;
  std::vector<Storage::FileSaved> answers_;
};

template <typename... Padds> class RunningSystem {
public:
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread mainThread;
  RunningSystem() {
    system.instantiate();
    mainThread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~RunningSystem() { stop(); }
  void stop() {
    if (mainThread.joinable()) {
      system.stop();
      mainThread.join();
    }
  }
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
};

std::unique_ptr<Storage::AppendFile> openLog(const std::string &name, unsigned writeEvery) {
  std::string error;
  auto file = Storage::AppendFile::open(root() + "/logs/" + name + ".log",
                                        {.name = name, .writeEvery = writeEvery}, error);
  EXPECT_TRUE(file) << error;
  return file;
}

} // namespace

// A source registered before it starts (the log, made in main) is written
// by it when the source rings; its tab shows where and a batch control
TEST(StoragePaddTest, WritesARegisteredSourceWhenRung) {
  auto log = openLog("log", 2);
  Storage::sources().add(log.get());
  {
    RunningSystem<TestStoragePadd> running;
    EXPECT_TRUE(running.padd<TestStoragePadd>().has("log"));
    EXPECT_TRUE(running.padd<TestStoragePadd>().has("log batch"));
    log->add("one\n");
    EXPECT_EQ(contentsOf(log->path()), "");
    log->add("two\n"); // rings it
    EXPECT_TRUE(eventually([&] { return contentsOf(log->path()) == "one\ntwo\n"; }));
    running.stop();
    log->add("after\n");
    log->add("stopped\n"); // its own batch again
    EXPECT_EQ(contentsOf(log->path()), "one\ntwo\nafter\nstopped\n");
  }
  Storage::sources().remove(log.get());
}

// A source registered while it runs is attached (and shown); removed, it's
// let go (and its controls go)
TEST(StoragePaddTest, AttachesSourcesAsTheyCome) {
  RunningSystem<TestStoragePadd> running;
  auto &padd = running.padd<TestStoragePadd>();
  auto recording = openLog("replay", 1);
  Storage::sources().add(recording.get());
  EXPECT_TRUE(eventually([&] { return padd.has("replay"); }));
  EXPECT_TRUE(padd.has("replay writes"));
  recording->add("frame 1\n");
  EXPECT_TRUE(eventually([&] { return contentsOf(recording->path()) == "frame 1\n"; }));
  Storage::sources().remove(recording.get());
  EXPECT_TRUE(eventually([&] { return !padd.has("replay"); }));
  recording.reset();                   // (gone, and the padd doesn't touch it)
  std::this_thread::sleep_for(1100ms); // a repeat() passes
}

// A padd's file saved whole under the app's folder, and answered; a path
// out of the folder refused
TEST(StoragePaddTest, SavesFilesForAPadd) {
  RunningSystem<TestStoragePadd, SaverPadd> running;
  auto &saver = running.padd<SaverPadd>();
  saver.save("saves/slot1.bin", "level 3");
  saver.save("../outside.bin", "no");
  ASSERT_TRUE(eventually([&] { return saver.answers().size() == 2; }));
  const auto answers = saver.answers();
  EXPECT_EQ(answers[0].path, "saves/slot1.bin");
  EXPECT_TRUE(answers[0].ok) << answers[0].error;
  EXPECT_EQ(contentsOf(root() + "/saves/slot1.bin"), "level 3");
  EXPECT_FALSE(answers[1].ok);
  EXPECT_FALSE(answers[1].error.empty());
  EXPECT_FALSE(std::filesystem::exists(root() + "/../outside.bin"));
}
