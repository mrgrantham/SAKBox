# Debug log: a console app's log, read on the Mac

A console's screens are its app's, so ScratchPadd's 3DS and DS apps log nothing there. A **debug build** (`bazel build -c dbg`, or `bazel run -c dbg`) sends every padd's log somewhere the Mac can read it:

| Platform | Where | How to read it |
| --- | --- | --- |
| 3DS | Kept in memory (the last 64 KB) and served over Wi-Fi on port 5005 | `python3 tools/console_log/console_log.py --3ds ADDRESS` ([`tools/console_log`](../../../tools/console_log/README.md)): what came before it connected, then each line as it's logged |
| DS | The flashcart's SD card, `fat:/logs/APP.log`, each line on the card as it's logged (written through libfat's cache with `fsync`, so switching the DS off keeps it), which costs a card write a line (a DS Lite can't join a WPA2 network) | Off the card |
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
  ...
}
```

- **Its Makefile** takes the sources (`"//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log"` in `extra_srcs`, `scratchpadd/debug_log/src/server scratchpadd/debug_log/src/session` in `SOURCES`, `scratchpadd/debug_log/include` in `INCLUDES`), and the debug switch: `CFLAGS += $(if $(SAKBOX_DEBUG),-DSAKBOX_DEBUG)`, before `CXXFLAGS := $(CFLAGS)`. A debug build runs `make SAKBOX_DEBUG=1` ([`tools/devkitpro`](../../../tools/devkitpro/README.md)). A DS app links `-lfat`.
- **Level:** info and above, or the level given (`Session("my-app", spdlog::level::debug)`). Debug adds a line for every message a padd sends, which costs a console time to format, and on a DS a write to the card each.
- **How:** `Session` routes every ScratchPadd logger (`Logger::routeTo`) to its destination instead of the terminal, which on a console is its screen: loggers made before it as well as after. When it ends, logging goes back to the terminal.

## Parts

| Target | What |
| --- | --- |
| `:log_server` | `LogBuffer` (a log kept in memory, the oldest lines dropped past its capacity, read from where each reader left off) and `LogServer` (a `LogBuffer` served to whoever connects to its port, one at a time: what's kept, then each line as it comes). Plain C++, no ScratchPadd or spdlog; on a 3DS it starts the network itself. Not on Windows or a DS. The 3DS benchmark and the audio self-test use them to serve their results once they're done (`:server_sources`) |
| `:debug_log` | `Session`, and `LogBufferSink` (an spdlog sink into a `LogBuffer`) |

| Test | Covers |
| --- | --- |
| `LogBuffer_test` | Readers getting what's new since their last read, the oldest whole lines dropped, a reader waiting for the next line, and every ScratchPadd logger routed into it and back |
| `LogServer_test` | On localhost: a reader getting what's kept and then each line, the next reader getting it all, and a port in use reported. Not on Windows |

Checked in Azahar, whose network is the Mac's: `console_log --3ds 127.0.0.1` read the 3DS demo's debug log from its first line, and the audio self-test's results.
