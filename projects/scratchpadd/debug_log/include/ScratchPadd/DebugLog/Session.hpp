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
// console time, and on a DS a card write each) then goes where the Mac can
// read it, instead of the screens:
// - 3DS: kept in memory (the last 64 KB) and served over Wi-Fi on port 5005
//   (LogServer), for tools/console_log to read, with what came before it
//   connected.
// - DS: written to the flashcart's SD card, fat:/logs/APP.log (a DS Lite
//   can't join a WPA2 network), in batches of cardWriteEvery lines, each
//   synced to the card (fsync), and at once for a warning or an error, so
//   a DS switched off keeps what was written (CardLog). With a
//   DebugLog::WriterPadd in the app's system, the writing is done on its
//   thread, when nothing else has work; before and after, as lines are
//   logged. The log says what syncs cost.
// - Desktops: the terminal, as always.
// Logging goes back to the terminal (and its level back) when it ends.
#pragma once

#include <spdlog/common.h>

#include <memory>
#include <string>

namespace ScratchPadd::DebugLog {

struct SessionOptions {
  // The lowest level logged
  spdlog::level::level_enum level = spdlog::level::info;
  // DS: how many lines are written to the card together (and synced once;
  // warnings and errors at once). 1 loses nothing when the DS is switched
  // off or hangs; more costs fewer card writes, for a chatty log, and can
  // lose the last few info lines. WriterPadd's "Lines between writes"
  // control changes it while the app runs.
  unsigned cardWriteEvery = 1;
};

class Session {
public:
  // e.g. Session("my-app", {.level = spdlog::level::debug, .cardWriteEvery = 8})
  explicit Session(const std::string &appName, SessionOptions options = {});
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
