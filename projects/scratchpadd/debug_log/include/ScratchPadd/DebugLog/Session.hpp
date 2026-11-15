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
// - desktops: the terminal, as always; and in a debug build (or with
//   `serve`), served over the network as the 3DS's is, on the first free
//   port from 5005, when the app lists a NetworkPadd;
// - 3DS: kept in memory (the last 64 KB) and served over Wi-Fi on port 5005
//   by NetworkPadd (a Network::StreamService), for tools/console_log to
//   read, with what came before it connected; the app lists a NetworkPadd;
// - DS: only the file (a DS Lite can't join a WPA2 network).
// Logging goes back to the terminal (and its level back) when it ends.
#pragma once

#include <ScratchPadd/Storage/Folders.hpp>

#include <spdlog/common.h>

#include <cstdint>
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
  // Desktops: also serve the log over the network, as a 3DS does (the last
  // 64 KB, then each line), when the app lists a NetworkPadd. On in a debug
  // build unless given (kServeByDefault); a 3DS always serves, a DS can't.
  std::optional<bool> serve;
};

// Whether a desktop serves its log unless told: in a debug build
// (-c dbg, or any build without NDEBUG)
#if defined(SAKBOX_DEBUG) || !defined(NDEBUG)
inline constexpr bool kServeByDefault = true;
#else
inline constexpr bool kServeByDefault = false;
#endif

class Session {
public:
  // The log's port (FBI and ftpd use 5000); a desktop takes the first free
  // one of kLogPorts from it, so several apps can serve at once: 5005, then
  // 5007 to 5015, skipping 5006, RemoteControlsPadd's (network's kPort)
  static constexpr std::uint16_t kLogPort = 5005;
  static constexpr std::uint16_t kLogPorts = 10;
  static constexpr std::uint16_t kSkippedPort = 5006;

  // e.g. Session({.level = spdlog::level::debug, .writeEvery = 8})
  explicit Session(const SessionOptions &options = {});
  ~Session();
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  // Where the log is going: its file, and where it's served (a 3DS, or a
  // desktop's debug build); or why not ("off: no SD card")
  const std::string &where() const { return where_; }
  // Where it's served ("192.168.4.25:5007"), or "" if it isn't
  const std::string &servedAt() const { return servedAt_; }
  // The log file's path ("" without one)
  std::string file() const;

private:
  struct Platform;
  std::unique_ptr<Platform> platform_;
  std::string where_;
  std::string servedAt_;
};

} // namespace ScratchPadd::DebugLog
