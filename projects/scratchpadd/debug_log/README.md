# Debug log: a console app's log, read on the Mac

A console's screens are its app's, so ScratchPadd's 3DS and DS apps log nothing there. A **debug build** (`bazel build -c dbg`, or `bazel run -c dbg`) sends every padd's log somewhere the Mac can read it:

| Platform | Where | How to read it |
| --- | --- | --- |
| 3DS | Kept in memory (the last 64 KB) and served over Wi-Fi on port 5005 | `python3 tools/console_log/console_log.py --3ds ADDRESS` ([`tools/console_log`](../../../tools/console_log/README.md)): what came before it connected, then each line as it's logged |
| DS | The flashcart's SD card, `fat:/logs/APP.log` (a DS Lite can't join a WPA2 network), written in batches and synced to the card (`fsync`, through libfat's cache), by `WriterPadd` when nothing else has work, so switching the DS off keeps what was written | Off the card |
| Desktops | The terminal, as always | |

The app shows where on its screen ("Log: 192.168.4.25:5005", "Log: fat:/logs/scratchpadd-ds-demo.log", or "Log: off: no SD card"). The ScratchPadd DS and 3DS demos and the Cyclone ragdoll on both do. A release build is unchanged: no log, no network.

## In an app

```cpp
#include <ScratchPadd/DebugLog/Session.hpp>   // //projects/scratchpadd/debug_log:sources

int main() {
  spdlog::set_level(spdlog::level::off);   // (a console app's usual)
#if defined(SAKBOX_DEBUG)
  ScratchPadd::DebugLog::Session log("my-app");   // before the padds start, for the app's life
  logWhere = log.where();                         // to show
#endif
  {
    // (WriterPadd: a DS's log written to the card when idle; nothing elsewhere)
    ScratchPadd::SystemImplementation<..., ScratchPadd::DebugLog::WriterPadd> system;
    ...
  }
}
```

- **Its Makefile** takes the sources (`"//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log"` in `extra_srcs`, `scratchpadd/debug_log/src/server scratchpadd/debug_log/src/session` in `SOURCES`, `scratchpadd/debug_log/include` in `INCLUDES`), and the debug switch: `CFLAGS += $(if $(SAKBOX_DEBUG),-DSAKBOX_DEBUG)`, before `CXXFLAGS := $(CFLAGS)`. A debug build runs `make SAKBOX_DEBUG=1` ([`tools/devkitpro`](../../../tools/devkitpro/README.md)). A DS app links `-lfat`.
- **Options** (`SessionOptions`, e.g. `Session("my-app", {.level = spdlog::level::debug, .cardWriteEvery = 8})`):
  - `level`: info and above by default. Debug adds a line for every message a padd sends, which costs a console time to format, and on a DS a card write each.
  - `cardWriteEvery` (DS): how many lines are written to the card together, 1 by default; WriterPadd's "Lines between writes" control changes it while the app runs. Warnings and errors are written at once either way. 1 loses nothing to a power-off or a hang; more costs fewer card writes for a chatty log, and can lose the last few info lines.
- **On a DS** (`CardLog`): a sync writes libfat's cache to the card (plain `fflush` only reaches the cache, which reaches the card when the file closes, so a DS switched off kept nothing), and costs a DS Lite about 7 ms, up to 25. So logging a line only adds it to the `CardLog` in memory, and the writing is done in batches, one sync each:
  - while the system runs, by **`WriterPadd`**, which `runs()` `WhenIdle`: on a thread of its own that only runs when nothing else has work, so no padd waits for the card. The `CardLog` rings its doorbell (a callback the padd gives it, which queues a write on the padd's own thread) every `cardWriteEvery` lines, and at once for a warning or an error; its `repeat()` writes what's left once a second. It publishes the `CardLog::Figures` (syncs, their cost, lines waiting) as shared state.
  - before the padds start and after they stop, by the `CardLog` itself, as lines are logged, and when the session ends.

  Every 16 syncs, and when it closes, the log says what they cost: `16 card syncs so far: 7.1 ms each, 24.2 ms at worst` (a DS Lite's figures), as a `DebugLog` logger's line.
- **How:** `Session` routes every ScratchPadd logger (`Logger::routeTo`) to its destination instead of the terminal, which on a console is its screen: loggers made before it as well as after. When it ends, logging goes back to the terminal.

## Parts

| Target | What |
| --- | --- |
| `:log_server` | `LogBuffer` (a log kept in memory, the oldest lines dropped past its capacity, read from where each reader left off) and `LogServer` (a `LogBuffer` served to whoever connects to its port, one at a time: what's kept, then each line as it comes). Plain C++, no ScratchPadd or spdlog; on a 3DS it starts the network itself. Not on Windows or a DS. The 3DS benchmark and the audio self-test use them to serve their results once they're done (`:server_sources`) |
| `:debug_log` | `Session`, `LogBufferSink` (an spdlog sink into a `LogBuffer`) and `CardLog` (a log file written in batches by whoever drains it; plain C++, tested on desktops) |
| `:writer_padd` | `WriterPadd`: writes a DS's `CardLog` when nothing else has work. Without one (a release build, a 3DS, a desktop) it has no timer or controls and does nothing |

| Test | Covers |
| --- | --- |
| `LogBuffer_test` | Readers getting what's new since their last read, the oldest whole lines dropped, a reader waiting for the next line, and every ScratchPadd logger routed into it and back |
| `CardLog_test` | Lines written in batches without a writer, the doorbell rung (once until written) with one, warnings at once, one sync per batch, detaching, and the figures written on closing |
| `WriterPadd_test` | In a running system: lines written by the padd when its doorbell rings, warnings at once, the log writing for itself again once it stops; idle without a log |
| `LogServer_test` | On localhost: a reader getting what's kept and then each line, the next reader getting it all, and a port in use reported. Not on Windows |

Checked in Azahar, whose network is the Mac's: `console_log --3ds 127.0.0.1` read the 3DS demo's debug log from its first line, and the audio self-test's results.
