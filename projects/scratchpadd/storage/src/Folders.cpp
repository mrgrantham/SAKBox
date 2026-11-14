#include <ScratchPadd/Storage/Folders.hpp>

#include <cstdlib>
#include <string>

#if defined(__NDS__)
#include <fat.h>
#endif
#if defined(__NDS__) || defined(__3DS__)
#include <cerrno>
#include <sys/stat.h>
#else
#include <filesystem>
#include <system_error>
#endif

namespace ScratchPadd::Storage {

namespace {

#if !defined(__NDS__) && !defined(__3DS__)
// $NAME, or `fallback` (under the home folder) when it isn't set
std::string environmentOr(const char *name, const std::string &fallback) {
  const char *value = std::getenv(name);
  if (value && *value)
    return value;
#if defined(_WIN32)
  const char *home = std::getenv("USERPROFILE");
#else
  const char *home = std::getenv("HOME");
#endif
  return std::string(home ? home : ".") + fallback;
}
#endif

std::string slashed(std::string path) {
  if (!path.empty() && path.back() != '/' && path.back() != '\\')
    path += '/';
  return path;
}

} // namespace

Folders Folders::forApp(const AppInfo &app) {
  const std::string &name = app.name;
#if defined(__NDS__)
  return {.files = "fat:/data/" + name + "/", .logs = "fat:/data/" + name + "/logs/"};
#elif defined(__3DS__)
  return {.files = "sdmc:/3ds/" + name + "/", .logs = "sdmc:/3ds/" + name + "/logs/"};
#elif defined(__APPLE__)
  const std::string home = environmentOr("HOME", "");
  return {.files = home + "/Library/Application Support/" + name + "/",
          .logs = home + "/Library/Logs/" + name + "/"};
#elif defined(_WIN32)
  const std::string local = slashed(environmentOr("LOCALAPPDATA", "\\AppData\\Local"));
  return {.files = local + name + "\\", .logs = local + name + "\\logs\\"};
#else
  return {.files = slashed(environmentOr("XDG_DATA_HOME", "/.local/share")) + name + "/",
          .logs = slashed(environmentOr("XDG_STATE_HOME", "/.local/state")) + name + "/"};
#endif
}

Folders Folders::under(std::string root) {
  root = slashed(std::move(root));
  return {.files = root, .logs = root + "logs/"};
}

bool ready(std::string &error) {
#if defined(__NDS__)
  // (libfat: the flashcart's card, through its DLDI driver; started once)
  static const bool started = fatInitDefault();
  if (!started)
    error = "no SD card";
  return started;
#else
  (void)error;
  return true;
#endif
}

bool makeFolder(const std::string &path) {
#if defined(__NDS__) || defined(__3DS__)
  // Each folder from the top ("fat:/data/", then "fat:/data/app/", ...);
  // the device's own ("fat:/") is skipped
  for (std::size_t at = path.find(":/"); at != std::string::npos;) {
    at = path.find('/', at + 2);
    const std::string folder = path.substr(0, at);
    if (folder.empty() || folder.back() == ':')
      continue;
    if (::mkdir(folder.c_str(), 0777) != 0 && errno != EEXIST)
      return false;
    if (at == std::string::npos)
      break;
  }
  return true;
#else
  std::error_code error;
  std::filesystem::create_directories(path, error);
  return !error && std::filesystem::is_directory(path, error);
#endif
}

} // namespace ScratchPadd::Storage
