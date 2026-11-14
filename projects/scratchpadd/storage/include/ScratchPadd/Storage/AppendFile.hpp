// AppendFile.hpp - a file that grows, written in batches: a log, a
// recording. Pieces are added from any thread and kept in memory, then
// written and synced together (one sync for many pieces: a sync costs a DS
// Lite about 7 ms, up to 25):
// - by StoragePadd (attached as a Source), when it rings its doorbell:
//   every `writeEvery` pieces;
// - by the thread adding a piece, at once, when it's urgent (a warning or
//   an error) or the oldest waiting has waited kLongestWait: StoragePadd
//   only runs when nothing else has work, and a busy app (a DS whose
//   physics never paused) kept it from writing for 22 s;
// - by the thread adding a piece, while nothing's attached (before the
//   padds start, after they stop).
// Opening it starts the file afresh and keeps the runs before: name.log
// becomes name.1.log, name.1.log name.2.log, and so on, keepRuns of them.
// Every kSyncsPerReport syncs, and when it closes, it adds a line of its
// figures (formatLine makes it look like the rest, a log's line).
#pragma once

#include <ScratchPadd/Storage/Source.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace ScratchPadd::Storage {

class AppendFile final : public Source {
public:
  struct Options {
    std::string name = "file"; // the Source's name ("log")
    unsigned writeEvery = 1;   // pieces written together
    unsigned keepRuns = 5;     // name.1.log ... name.5.log
    // The file's own line (its figures) as the rest look: a log's line
    std::function<std::string(std::string_view text)> formatLine;
  };
  // What its writing has cost: the syncs, and how long pieces waited for
  // StoragePadd to write them (long when the app is busy)
  struct Figures {
    std::uint32_t syncs = 0;
    float meanSyncMilliseconds = 0, worstSyncMilliseconds = 0;
    float worstWaitMilliseconds = 0; // from the doorbell to the write
    std::uint32_t written = 0;       // pieces written by the thread adding them
    std::uint32_t waiting = 0;
  };
  static constexpr unsigned kSyncsPerReport = 16;
  static constexpr std::chrono::seconds kLongestWait{2};

  // The file at `path` (its folder made, the runs before kept), or nullptr
  // with `error` saying why
  static std::unique_ptr<AppendFile> open(const std::string &path, Options options,
                                          std::string &error);
  ~AppendFile() override; // writes what's waiting, and the figures
  AppendFile(const AppendFile &) = delete;
  AppendFile &operator=(const AppendFile &) = delete;

  // A piece (a formatted line, with its newline). Safe from any thread.
  void add(std::string_view piece, bool urgent = false);

  // Source
  std::string name() const override { return name_; }
  std::string where() const override { return path_; }
  std::string figuresLine() const override;
  void attach(std::function<void()> ring) override;
  void detach() override;
  void write() override;
  std::optional<unsigned> writeEvery() const override;
  void setWriteEvery(unsigned pieces) override;

  Figures figures() const;
  const std::string &path() const { return path_; }

  // `path`'s name for run `run` before this one: logs/app.log, 2 ->
  // logs/app.2.log
  static std::string runPath(const std::string &path, unsigned run);

private:
  using Clock = std::chrono::steady_clock;
  AppendFile(std::FILE *file, std::string path, Options options);
  void writeWaiting(bool byAdder); // under fileMutex_
  void report();                   // under fileMutex_

  // What's waiting, the doorbell and when to ring it
  mutable std::mutex mutex_;
  std::string waiting_;
  std::uint32_t piecesWaiting_ = 0;
  Clock::time_point oldestWaiting_{};
  unsigned writeEvery_;
  std::function<void()> ring_;
  bool rung_ = false;
  Clock::time_point rungAt_{};

  // The file and what writing it has cost
  mutable std::mutex fileMutex_;
  std::FILE *file_;
  const std::string path_, name_;
  std::function<std::string(std::string_view)> formatLine_;
  std::uint32_t syncs_ = 0, writtenByAdder_ = 0;
  Clock::duration totalSync_{}, worstSync_{}, worstWait_{};
};

} // namespace ScratchPadd::Storage
