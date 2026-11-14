# Storage: an app's files, written when nothing else has work

Writing to a card or disk is slow. A sync costs a DS Lite about 7 ms, up to 25, and a busy app can't afford to wait for it. So ScratchPadd keeps an app's writing in one place, **`StoragePadd`**. It `runs()` `WhenIdle`, so it only gets the processor when no other padd has work, and it does two jobs:

- **Sources**: things that keep their own writes waiting, such as the app's log (`DebugLog::Session`), and later a recording. A source rings StoragePadd's doorbell when it's time to write. StoragePadd then does the writing on its own thread, and also writes every source once a second.
- **Saving whole files** for any padd: `SaveFile{path, bytes}`, answered with `FileSaved{path, ok, error}`.

```cpp
#include <ScratchPadd/Storage/StoragePadd.hpp>   // //projects/scratchpadd/storage:storage_padd

ScratchPadd::SystemImplementation<GamePadd, ScreenPadd, ..., ScratchPadd::Storage::StoragePadd> system;

// In a padd: save, and hear back
sendTo(addressOf("StoragePadd"),
       Storage::SaveFile{.path = "saves/slot1.bin",
                         .bytes = std::make_shared<const Storage::Bytes>(data)});
receive<Storage::FileSaved>([](const Storage::FileSaved &saved) { /* saved.ok, saved.error */ });

// Loading, whenever the app wants it (not idle-time work)
std::optional<Storage::Bytes> slot = Storage::loadFile(Storage::Folders::forApp().files + "saves/slot1.bin");
```

## Where an app's files go

`Folders::forApp()` follows each platform's usual places, named after the app (`AppInfo::name`, the BUILD target's name). The toolkit the app was made with isn't named in them:

| Platform | Files | Logs |
| --- | --- | --- |
| DS | `fat:/data/<app>/` (homebrew's usual place) | `fat:/data/<app>/logs/` |
| 3DS | `sdmc:/3ds/<app>/` (where 3DS homebrew keeps its data) | `sdmc:/3ds/<app>/logs/` |
| macOS | `~/Library/Application Support/<app>/` | `~/Library/Logs/<app>/` |
| Linux | `$XDG_DATA_HOME/<app>/` (`~/.local/share`) | `$XDG_STATE_HOME/<app>/` (`~/.local/state`) |
| Windows | `%LOCALAPPDATA%\<app>\` | `%LOCALAPPDATA%\<app>\logs\` |

Folders are made when they're first written to. A `SaveFile` path is relative to the files folder, and one that climbs out of it (`..`, or an absolute path) is refused. On a DS, `ready()` starts the flashcart's card (libfat) the first time anything is written. `Folders::under(root)` puts both under one folder, for tests and tools.

## Saving whole files

`saveFile(path, bytes, error)` writes `path.tmp`, syncs it, and only then puts it in place of `path`. On desktops that's a rename, which replaces the old file in one step. A DS's or 3DS's rename won't replace a file, so the old one is removed first. If the power goes between the two steps, the new file is left whole as `path.tmp`, and `loadFile()` takes that when `path` is gone. Either way a power-off never leaves half a file.

## Files that grow: `AppendFile`

A log or a recording: pieces are added from any thread and kept in memory, then written and synced together, one sync for many pieces.

- **When StoragePadd is attached**, it writes them when the file rings its doorbell, every `writeEvery` pieces.
- **The thread adding a piece writes at once** when the piece is urgent (a warning or an error), or when the oldest waiting piece has waited `kLongestWait` (2 s). StoragePadd only runs when nothing else has work, and in the Cyclone ragdoll's sweep on a DS Lite, with the physics 96% busy, its writes waited up to 22 s to run.
- **With nothing attached** (before the padds start, after they stop), the thread adding a piece writes each batch.
- **Opening it keeps the runs before:** `app.log` becomes `app.1.log`, `app.1.log` becomes `app.2.log`, and so on, `keepRuns` of them (5 by default).
- Every 16 syncs, and when it closes, it adds a line of its figures: what the syncs cost, how long StoragePadd's writes waited to run, and how many pieces were written at once instead.

## StoragePadd's tab

On a controls panel (a handheld's SELECT, or a desktop's panels), StoragePadd's tab shows:
- **Files:** where the app's files go.
- **For each source** (the log is `log`):
  - its file (`log`);
  - what its writing costs (`log writes`), updated each second;
  - a control for the pieces it writes together (`log batch`, 1 to 256).

These are `Info` controls, shown and not edited, apart from the batch control. On a DS, a path too long for its row wraps onto the rows below.

## Parts

| Target | What |
| --- | --- |
| `:storage` | `Folders`, `Files.hpp` (`saveFile`, `loadFile`, `sync`), `AppendFile`, and `Source` with the registry `sources()`. Plain C++, no padds |
| `:storage_padd` | `StoragePadd` and its messages (`SaveFile`, `FileSaved`) |
| `:sources` | Everything, for the consoles' Makefile builds (`FOLDER/src` in `SOURCES`, `FOLDER/include` in `INCLUDES`) |

StoragePadd uses a source only while holding the registry's lock (`Sources::forEach`, `with`). So once `sources().remove(&source)` returns, StoragePadd can't be using it, and the source can be destroyed.

| Test | Covers |
| --- | --- |
| `Folders_test` | Each platform's places named after the app, with no other name in them; Linux following `XDG_DATA_HOME` and `XDG_STATE_HOME`; folders under a root, made as needed |
| `Files_test` | A save replacing a file whole, with no `.tmp` left behind; an unfinished save loaded from its `.tmp`; nothing loading as nothing; a save that can't be written saying why |
| `AppendFile_test` | Batches written with nothing attached; the doorbell rung (once until written) when attached; urgent pieces written at once by the thread adding them; a piece written when the oldest has waited too long; the runs before kept; the figures written on closing; its `Source` side |
| `StoragePadd_test` | In a running system: a source registered before it starts written when it rings, with its controls; sources registered while it runs attached and shown, and let go when removed; files saved for a padd and answered; a path out of the folder refused; sources writing for themselves again once it stops |

All of them also ran 10 times each under ThreadSanitizer.
