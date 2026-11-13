// Session.hpp - a debug build's log, off the device. In main, before the
// padds start, for the app's life:
//
//   #if defined(SAKBOX_DEBUG)          // bazel build -c dbg
//   ScratchPadd::DebugLog::Session log("scratchpadd-3ds-demo");
//   // log.where(): "192.168.4.25:5005", for the app to show
//   #endif
//
// Every padd's log (every ScratchPadd logger, at info level and above, or
// the level given: debug adds a line for every message sent, which costs a
// console time, and on a DS a write to the card each) then goes where the
// Mac can read it, instead of the screens:
// - 3DS: kept in memory (the last 64 KB) and served over Wi-Fi on port 5005
//   (LogServer), for tools/console_log to read, with what came before it
//   connected.
// - DS: written to the flashcart's SD card, fat:/logs/APP.log (a DS Lite
//   can't join a WPA2 network), each line as it's logged.
// - Desktops: the terminal, as always.
// Logging goes back to the terminal (and its level back) when it ends.
#pragma once

#include <spdlog/common.h>

#include <memory>
#include <string>

namespace ScratchPadd::DebugLog {

class Session {
public:
  explicit Session(const std::string &appName,
                   spdlog::level::level_enum level = spdlog::level::info);
  ~Session();
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  // Where the log is: an address and port, a file, or "the terminal"; or
  // why it couldn't be sent ("off: no SD card")
  const std::string &where() const { return where_; }

private:
  struct Platform;
  std::unique_ptr<Platform> platform_;
  std::string where_;
};

} // namespace ScratchPadd::DebugLog
