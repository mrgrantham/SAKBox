# Log: an app's log, kept where it can be read afterwards

An app opens a log `Session` in `main`, before its padds start. Every ScratchPadd logger then writes to a **file in the app's log folder**, named after the app (`<app>.log`, the five runs before kept as `<app>.1.log` to `<app>.5.log`). Each platform adds more:

| Platform | Besides the file | Log folder | How to read it |
| --- | --- | --- | --- |
| Desktops | The terminal, as always | `~/Library/Logs/<app>/` on a Mac, `$XDG_STATE_HOME/<app>/` on Linux, `%LOCALAPPDATA%\<app>\logs\` on Windows | The terminal, or the file |
| 3DS | Kept in memory (the last 64 KB) and served over Wi-Fi on port 5005 | `sdmc:/3ds/<app>/logs/` | `python3 tools/console_log/console_log.py --3ds ADDRESS` ([`tools/console_log`](../../../tools/console_log/README.md)): what came before it connected, then each line as it's logged. Or the file, off the SD card |
| DS | Nothing (a DS Lite can't join a WPA2 network) | `fat:/data/<app>/logs/` | Off the flashcart's card |

The folders are [storage's](../storage/README.md#where-an-apps-files-go). The app's name is its BUILD target's (`AppInfo`), so `cyclone-ds-ragdoll` logs to `fat:/data/cyclone-ds-ragdoll/logs/cyclone-ds-ragdoll.log`. The first line says which app and build wrote it: `Cyclone ragdoll (cyclone-ds-ragdoll, debug build): logging to ...`.

The desktop apps open a session in every build. The console apps do so in a **debug build** (`bazel build -c dbg`), and show where on their screen. A release build on a console logs nothing: the screens are the app's, and a log costs a console time.

## In an app

```cpp
#include <ScratchPadd/DebugLog/Session.hpp>         // //projects/scratchpadd/debug_log
#include <ScratchPadd/Storage/StoragePadd.hpp>      // //projects/scratchpadd/storage:storage_padd

int main() {
#if defined(SAKBOX_DEBUG)       // (on a console; a desktop app opens one always)
  ScratchPadd::DebugLog::Session log;               // before the padds start, for the app's life
  logWhere = log.where();                           // to show
#endif
  {
    // (StoragePadd: the log written when nothing else has work)
    ScratchPadd::SystemImplementation<..., ScratchPadd::Storage::StoragePadd> system;
    ...
  }
}
```

- **A console app's Makefile** takes the sources:
  - in `extra_srcs`: `"//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log"` and `"//projects/scratchpadd/storage:sources": "scratchpadd/storage"`;
  - in `SOURCES`: `scratchpadd/debug_log/src/server scratchpadd/debug_log/src/session scratchpadd/storage/src`;
  - in `INCLUDES`: `scratchpadd/debug_log/include scratchpadd/storage/include`.

  It also needs the build's switches before `CXXFLAGS := $(CFLAGS)`: `CFLAGS += $(if $(SAKBOX_DEBUG),-DSAKBOX_DEBUG)` and `CFLAGS += $(SAKBOX_DEFINES)`, the second giving the app its name and title ([`tools/devkitpro`](../../../tools/devkitpro/README.md)). A DS app links `-lfat`.
- **Options** (`SessionOptions`, e.g. `Session({.level = spdlog::level::debug, .writeEvery = 8})`):
  - `level`: info and above by default. Debug adds a line for every message a padd sends, which costs a console time to format, and on a DS a card write each.
  - `terminalLevel` (desktops): the lowest level the terminal shows, `level` unless given. Precision Flight and the Cyclone padds example show only warnings, keeping the terminal for their own events, while the file gets everything.
  - `writeEvery`: how many lines are written to the file together, 1 by default. StoragePadd's "log batch" control changes it while the app runs. Warnings and errors are written at once either way. 1 loses nothing to a power-off or a hang; more costs fewer card writes for a chatty log, and can lose the last few info lines.
  - `keepRuns`: the runs kept beside it, 5 by default.
  - `folders`: somewhere else for the file (tests).
- **How the file is written:** it's an [`AppendFile`](../storage/README.md#files-that-grow-appendfile), registered as a storage source. Logging a line only adds it to the file's batch in memory. StoragePadd writes the batches on its own thread, when nothing else has work. Warnings, errors, and lines whose batch has waited 2 s are written at once by the thread logging them, so a busy app can't keep its log from the card. Before the padds start and after they stop, the thread logging writes each batch. Every 16 syncs, and when it closes, the file gets a line of what the syncs cost, as a `[Storage]` logger's: `16 syncs so far: 7.1 ms each, 24.2 ms at worst; StoragePadd's writes waited 3 ms at worst to run; 0 written at once`.
- **How it routes:** `Session` points every ScratchPadd logger (`Logger::routeTo`) at its file and the rest, instead of or as well as the terminal: loggers made before it as well as after. When it ends, logging goes back to the terminal.

## Parts

| Target | What |
| --- | --- |
| `:log_server` | `LogBuffer` (a log kept in memory, the oldest lines dropped past its capacity, read from where each reader left off) and `LogServer` (a `LogBuffer` served to whoever connects to its port, one at a time: what's kept, then each line as it comes). Plain C++, no ScratchPadd or spdlog; on a 3DS it starts the network itself. Not on Windows or a DS. The 3DS benchmark and the audio self-test use them to serve their results once they're done (`:server_sources`) |
| `:debug_log` | `Session` and `LogBufferSink` (an spdlog sink into a `LogBuffer`) |

| Test | Covers |
| --- | --- |
| `Session_test` | On a desktop: every logger's lines in a file named after the app in the log folder, warnings included; the file registered as a storage source while the session lasts; the next run starting afresh with the last kept beside it |
| `LogBuffer_test` | Readers getting what's new since their last read, the oldest whole lines dropped, a reader waiting for the next line, and every ScratchPadd logger routed into it and back |
| `LogServer_test` | On localhost: a reader getting what's kept and then each line, the next reader getting it all, and a port in use reported. Not on Windows |

Checked in melonDS (the Cyclone ragdoll's log in `fat:/data/cyclone-ds-ragdoll/logs/`, written by StoragePadd), in Azahar (the 3DS demo's both at `sdmc:/3ds/scratchpadd-3ds-demo/logs/` and over Wi-Fi), and on a Mac (`~/Library/Logs/cyclone_padds/`, with the terminal showing only warnings).
