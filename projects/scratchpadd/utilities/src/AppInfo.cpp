#include <ScratchPadd/AppInfo.hpp>

#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <cstdlib> // getprogname
#elif defined(__linux__)
#include <cerrno> // program_invocation_short_name
#endif

namespace ScratchPadd {

namespace {

// The executable's name, without its folder or (on Windows) ".exe"
std::string executableName() {
#if defined(_WIN32)
  char path[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, path, MAX_PATH);
  std::string name = path;
  name = name.substr(name.find_last_of("\\/") + 1);
  if (name.size() > 4 && name.ends_with(".exe"))
    name.resize(name.size() - 4);
  return name;
#elif defined(__APPLE__)
  return getprogname();
#elif defined(__linux__)
  return program_invocation_short_name;
#else
  return "app"; // (a console app's build always names it)
#endif
}

AppInfo find() {
  AppInfo app;
#if defined(SCRATCHPADD_APP_NAME)
  app.name = SCRATCHPADD_APP_NAME;
#else
  app.name = executableName();
#endif
#if defined(SCRATCHPADD_APP_TITLE)
  app.title = SCRATCHPADD_APP_TITLE;
#else
  app.title = app.name;
#endif
#if defined(SAKBOX_DEBUG) || !defined(NDEBUG)
  app.build = "debug";
#else
  app.build = "release";
#endif
#if defined(__NDS__)
  app.platform = "Nintendo DS";
#elif defined(__3DS__)
  app.platform = "Nintendo 3DS";
#elif defined(__APPLE__)
  app.platform = "macOS";
#elif defined(_WIN32)
  app.platform = "Windows";
#elif defined(__linux__)
  app.platform = "Linux";
#else
  app.platform = "unknown";
#endif
  return app;
}

} // namespace

const AppInfo &AppInfo::current() {
  static const AppInfo app = find();
  return app;
}

} // namespace ScratchPadd
