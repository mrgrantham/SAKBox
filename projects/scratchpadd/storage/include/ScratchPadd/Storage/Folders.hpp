// Folders.hpp - where an app keeps its files and its logs, each platform's
// usual place, named after the app (AppInfo::name):
//
//   DS        fat:/data/<app>/                       logs: fat:/data/<app>/logs/
//   3DS       sdmc:/3ds/<app>/                       logs: sdmc:/3ds/<app>/logs/
//   macOS     ~/Library/Application Support/<app>/   logs: ~/Library/Logs/<app>/
//   Linux     $XDG_DATA_HOME/<app>/ (~/.local/share)  logs: $XDG_STATE_HOME/<app>/ (~/.local/state)
//   Windows   %LOCALAPPDATA%\<app>\                  logs: %LOCALAPPDATA%\<app>\logs\
//
// Paths end in a separator. Nothing is made until it's used (makeFolder);
// on a DS, ready() starts the card first.
#pragma once

#include <ScratchPadd/AppInfo.hpp>

#include <string>

namespace ScratchPadd::Storage {

struct Folders {
  std::string files; // the app's own files (saves, settings)
  std::string logs;

  // The app's, in its platform's usual places
  static Folders forApp(const AppInfo &app = AppInfo::current());
  // Both under `root` (files, and logs in logs/), for tests and tools
  static Folders under(std::string root);
};

// Whether files can be written at all: on a DS, the flashcart's card started
// (libfat, once); false with `error` saying why. Elsewhere, true.
bool ready(std::string &error);

// Makes `path` and the folders above it, as needed. False if it can't.
bool makeFolder(const std::string &path);

} // namespace ScratchPadd::Storage
