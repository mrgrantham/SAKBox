// StoragePadd.hpp - an app's writing to its card or disk, done when nothing
// else has work (it runs WhenIdle), so no padd waits for storage:
// - for each Source registered (sources(): the log's file, a recording),
//   writing what's waiting when the source rings its doorbell, and every
//   source once a second;
// - for any padd, a whole file saved (SaveFile), answered with FileSaved.
// List it in an app's system:
//
//   ScratchPadd::SystemImplementation<GamePadd, ScreenPadd, ...,
//                                     ScratchPadd::Storage::StoragePadd> system;
//
// Its tab on a panel shows where the app's files go, and for each source
// where it writes and what its writing costs (Info controls), with a
// "lines between writes" control for each that batches.
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>
#include <ScratchPadd/Storage/Messages.hpp>
#include <ScratchPadd/Storage/Source.hpp>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::Storage {

class StoragePadd : public Base {
public:
  explicit StoragePadd(System *system, Folders folders = Folders::forApp())
      : Base(system), folders_(std::move(folders)) {
    receive<SaveFile>([this](const SaveFile &save, PaddAddress from) {
      FileSaved saved{.path = save.path};
      if (save.path.empty() || save.path.find("..") != std::string::npos ||
          save.path.front() == '/' || save.path.front() == '\\' ||
          save.path.find(':') != std::string::npos) {
        saved.error = "not a path in the app's folder: " + save.path;
      } else if (!save.bytes) {
        saved.error = "nothing to save";
      } else {
        saved.ok = saveFile(folders_.files + save.path, *save.bytes, saved.error);
      }
      if (!saved.ok)
        logger().warn("Couldn't save {}: {}", save.path, saved.error);
      if (from)
        sendTo(from, saved);
    });
  }
  std::string name() override { return "StoragePadd"; }
#if SCRATCHPADD_HAS_THREADS
  Runs runs() override { return Runs::WhenIdle; }
#endif
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::seconds(1);
  }
  // Where the app's files go, and each source registered by now (the log's,
  // made in main); sources registered later add theirs as they come
  ControlValueMap generateControls() override {
    ControlValueMap controls{{kFiles, ControlType::Info(folders_.files)}};
    for (Source *source : sources().all())
      for (auto &[name, control] : controlsFor(*source))
        controls.emplace(name, control);
    return controls;
  }
  void controlChanged(const std::string &name, const ControlTypeVariant &) override {
    for (const Attached &attached : attached_) {
      if (name == attached.name + kBatchSuffix) {
        const auto pieces = static_cast<unsigned>(control<ControlType::Integer>(name).value);
        sources().with(attached.source, [&](Source &source) { source.setWriteEvery(pieces); });
      }
    }
  }

  void starting() override {
    // (called on the registering thread: catch up on this padd's own)
    sources().listen([this] { push([this] { catchUp(); }); });
    catchUp();
  }
  void finishing() override {
    sources().listen(nullptr);
    sources().forEach([](Source &source) {
      source.detach();
      source.write();
    });
    attached_.clear();
  }
  void repeat() override {
    sources().forEach([](Source &source) { source.write(); });
    showFigures();
  }

  const Folders &folders() const { return folders_; }

private:
  static constexpr const char *kFiles = "Files";
  static constexpr const char *kBatchSuffix = " batch";   // "log batch": pieces between writes
  static constexpr const char *kWritesSuffix = " writes"; // "log writes": its figures

  struct Attached {
    const Source *source;
    std::string name;
  };

  static ControlValueMap controlsFor(const Source &source) {
    ControlValueMap controls{
        {source.name(), ControlType::Info(source.where())},
        {source.name() + kWritesSuffix, ControlType::Info(source.figuresLine())}};
    if (const auto every = source.writeEvery())
      controls.emplace(
          source.name() + kBatchSuffix,
          ControlType::Integer(static_cast<std::int32_t>(*every), std::make_pair(1, 256)));
    return controls;
  }

  // Attaches to sources registered since, and lets go of those removed
  void catchUp() {
    std::vector<Attached> now;
    sources().forEach([&](Source &source) {
      const bool known = std::ranges::any_of(
          attached_, [&](const Attached &attached) { return attached.source == &source; });
      if (!known) {
        // (the doorbell: rung from whichever thread adds to the source, it
        // queues the source's write here, on this padd's thread)
        const Source *which = &source;
        source.attach([this, which] {
          push([which] { sources().with(which, [](Source &s) { s.write(); }); });
        });
      }
      now.push_back({&source, source.name()});
    });
    for (const Attached &before : attached_) {
      const bool kept = std::ranges::any_of(
          now, [&](const Attached &attached) { return attached.source == before.source; });
      if (!kept)
        for (const char *suffix : {"", kWritesSuffix, kBatchSuffix})
          removeControl(before.name + suffix);
    }
    for (const Attached &added : now) {
      const bool known = std::ranges::any_of(
          attached_, [&](const Attached &attached) { return attached.source == added.source; });
      if (!known)
        sources().with(added.source, [&](Source &source) {
          for (auto &[name, control] : controlsFor(source))
            if (!hasControl(name))
              addControl(name, control);
        });
    }
    attached_ = std::move(now);
  }

  // Each source's figures, where they've changed
  void showFigures() {
    for (const Attached &attached : attached_) {
      sources().with(attached.source, [&](Source &source) {
        const std::string name = attached.name + kWritesSuffix;
        std::string line = source.figuresLine();
        if (hasControl(name) && control<ControlType::Info>(name).value != line)
          setControl(name, ControlType::Info(std::move(line)));
      });
    }
  }

  Folders folders_;
  std::vector<Attached> attached_; // (only compared: used through sources())
};

} // namespace ScratchPadd::Storage
