// Session.hpp - an app's log, kept where it can be read afterwards. In main,
// before the padds start, for the app's life:
//
//   ScratchPadd::DebugLog::Session log;   // on a console, in a debug build (bazel build -c dbg)
//   // log.where(): where it's going, for the app to show
//
// Every padd's log (every ScratchPadd logger, at info level and above, or
// the level given: debug adds a line for every message sent, which costs a
// console time, and on a DS a card write each) is then written to a file in
// the app's log folder (Storage::Folders: fat:/data/<app>/logs/ on a DS,
// ~/Library/Logs/<app>/ on a Mac, ...), <app>.log, the runs before kept as
// <app>.1.log and on (Storage::AppendFile). The file is a Storage source:
// with a StoragePadd in the app's system, it's written on that padd's
// thread, when nothing else has work, in batches of writeEvery lines;
// warnings and errors, and lines that waited 2 s, are written at once.
// And besides the file:
// - desktops: the terminal, as always;
// - 3DS: kept in memory (the last 64 KB) and served over Wi-Fi on port 5005
//   (LogServer), for tools/console_log to read, with what came before it
//   connected;
// - DS: only the file (a DS Lite can't join a WPA2 network).
// Logging goes back to the terminal (and its level back) when it ends.
#pragma once

#include <ScratchPadd/Storage/Folders.hpp>

#include <spdlog/common.h>

#include <memory>
#include <optional>
#include <string>

namespace ScratchPadd::DebugLog {

struct SessionOptions {
  // The lowest level logged
  spdlog::level::level_enum level = spdlog::level::info;
  // Desktops: the lowest level the terminal shows (an app keeping its
  // terminal for its own events, say warn); `level` unless given
  std::optional<spdlog::level::level_enum> terminalLevel;
  // How many lines are written to the file together (and synced once).
  // 1 loses nothing when a console is switched off or hangs; more costs
  // fewer card writes, for a chatty log, and can lose the last few info
  // lines. StoragePadd's "log batch" control changes it while the app runs.
  unsigned writeEvery = 1;
  // The runs before kept beside it: <app>.1.log ... <app>.5.log
  unsigned keepRuns = 5;
  // Where the log file goes: the app's own folders unless given (tests)
  std::optional<Storage::Folders> folders;
};

class Session {
public:
  // e.g. Session({.level = spdlog::level::debug, .writeEvery = 8})
  explicit Session(const SessionOptions &options = {});
  ~Session();
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  // Where the log is going: its file, and on a 3DS its address and port; or
  // why not ("off: no SD card")
  const std::string &where() const { return where_; }
  // The log file's path ("" without one)
  std::string file() const;

private:
  struct Platform;
  std::unique_ptr<Platform> platform_;
  std::string where_;
};

} // namespace ScratchPadd::DebugLog
