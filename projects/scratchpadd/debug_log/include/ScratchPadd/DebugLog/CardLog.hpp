#pragma once

// CardLog.hpp - a log file written to a card in batches, for a DS's debug
// log (Session). Lines are added from any thread and kept in memory; they're
// written to the file and synced to the card together (one sync for many
// lines: a sync costs a DS Lite about 7 ms, up to 25):
// - by a writer attached to it (WriterPadd, on a thread that runs when
//   nothing else has work), whose doorbell it rings every `writeEvery` lines,
//   and at once for a warning or an error;
// - by itself, as they're added, while no writer is attached: before the
//   padds start and after they stop.
// Syncing (fsync) matters on a DS: fflush only reaches libfat's cache, which
// reaches the card when the file closes, so a DS switched off kept nothing.
// What's been synced survives a DS being switched off; what's waiting
// doesn't. Every kSyncsPerReport syncs, and when it closes, the file gets a
// line of figures: how many syncs, and what they cost.
//
// Plain C++ (a FILE and fsync), so it's tested on desktops too.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace ScratchPadd::DebugLog {

class CardLog {
public:
  // What the syncs have cost so far, and what's waiting (WriterPadd
  // publishes it as shared state, so it's plain numbers)
  struct Figures {
    std::uint32_t syncs = 0;
    float meanMilliseconds = 0;
    float worstMilliseconds = 0;
    std::uint32_t linesWaiting = 0;
  };
  // Makes a line of the log's own (its figures) look like a logged one:
  // the "DebugLog" logger's, at info level
  using LineFormatter = std::function<std::string(std::string_view text)>;

  static constexpr unsigned kSyncsPerReport = 16;

  // The log at `path` (made anew), or nullptr with `error` saying why
  static std::unique_ptr<CardLog> open(const std::string &path, unsigned writeEvery,
                                       LineFormatter formatLine, std::string &error);
  ~CardLog(); // writes what's waiting, and the figures
  CardLog(const CardLog &) = delete;
  CardLog &operator=(const CardLog &) = delete;

  // A line, formatted and ending in a newline. `urgent` (a warning or an
  // error) has it written now. Safe from any thread; doesn't wait for a
  // sync in progress unless no writer is attached.
  void add(std::string_view line, bool urgent);

  // The writer's doorbell: rung (from whichever thread adds the line) when
  // it's time to write. Rung at most once until write() runs.
  void attach(std::function<void()> ringWriter);
  // After this returns the doorbell isn't rung again, and lines are
  // written as they're added
  void detach();
  // Writes what's waiting and syncs it (nothing if nothing's waiting)
  void write();

  unsigned writeEvery() const;
  void setWriteEvery(unsigned lines);
  Figures figures() const;

  // The one a debug Session opened, for WriterPadd to find (nullptr without)
  static CardLog *active();
  static void setActive(CardLog *log);

private:
  using Clock = std::chrono::steady_clock;
  CardLog(std::FILE *file, unsigned writeEvery, LineFormatter formatLine);
  void writeWaiting(); // write(), under fileMutex_
  void report();       // the figures, under fileMutex_

  // The lines waiting, the writer and when to ring it
  mutable std::mutex mutex_;
  std::string waiting_;
  std::uint32_t linesWaiting_ = 0;
  unsigned writeEvery_;
  std::function<void()> ringWriter_;
  bool rung_ = false;

  // The file and what syncing it has cost
  mutable std::mutex fileMutex_;
  std::FILE *file_;
  LineFormatter formatLine_;
  std::uint32_t syncs_ = 0;
  Clock::duration totalSync_{}, worstSync_{};
};

} // namespace ScratchPadd::DebugLog
