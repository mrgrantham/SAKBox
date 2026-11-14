// AppInfo.hpp - what an app is: its name, its title, how it was built and
// what it runs on. The build gives it, not the app's code: a console app's
// build (//tools/devkitpro) defines SCRATCHPADD_APP_NAME and
// SCRATCHPADD_APP_TITLE from its BUILD target and Makefile; elsewhere the
// name is the executable's (in Bazel, the target's name) and the title the
// same. A padd's log file, its storage folder (Storage::Folders) and its
// About tab (AboutPadd) all come from it.
//
//   const ScratchPadd::AppInfo &app = ScratchPadd::AppInfo::current();
//   app.name     // "cyclone-ds-ragdoll"
//   app.title    // "Cyclone ragdoll"
#pragma once

#include <string>

namespace ScratchPadd {

struct AppInfo {
  std::string name;     // for files and folders: the BUILD target's name
  std::string title;    // for people: the app's title
  std::string build;    // "debug" or "release"
  std::string platform; // "Nintendo DS", "Nintendo 3DS", "macOS", "Linux", "Windows"

  // This app's, worked out once
  static const AppInfo &current();
};

} // namespace ScratchPadd
