#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>

#include <cstdio>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
#if !defined(__NDS__) && !defined(__3DS__)
#include <filesystem>
#include <system_error>
#endif

namespace ScratchPadd::Storage {

void sync(std::FILE *file) {
  std::fflush(file);
#if defined(_WIN32)
  _commit(_fileno(file));
#else
  ::fsync(::fileno(file));
#endif
}

bool saveFile(const std::string &path, std::span<const std::uint8_t> bytes, std::string &error) {
  if (!ready(error))
    return false;
  const auto slash = path.find_last_of("/\\");
  if (slash != std::string::npos && !makeFolder(path.substr(0, slash + 1))) {
    error = "can't make the folder for " + path;
    return false;
  }
  const std::string temporary = path + ".tmp";
  std::FILE *file = std::fopen(temporary.c_str(), "wb");
  if (!file) {
    error = "can't write " + temporary;
    return false;
  }
  const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  sync(file);
  std::fclose(file);
  if (!written) {
    std::remove(temporary.c_str());
    error = "couldn't write all of " + temporary + " (is the card full?)";
    return false;
  }
#if defined(__NDS__) || defined(__3DS__)
  // (their rename won't replace a file: the old goes first, and loadFile()
  // takes the .tmp if a power-off comes between)
  std::remove(path.c_str());
  if (std::rename(temporary.c_str(), path.c_str()) != 0) {
    error = "can't put " + temporary + " in place of " + path;
    return false;
  }
#else
  std::error_code renameError;
  std::filesystem::rename(temporary, path, renameError); // (replaces it, everywhere)
  if (renameError) {
    error = "can't put " + temporary + " in place of " + path + ": " + renameError.message();
    return false;
  }
#endif
  return true;
}

std::optional<Bytes> loadFile(const std::string &path) {
  std::string error;
  if (!ready(error))
    return std::nullopt;
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file)
    file = std::fopen((path + ".tmp").c_str(), "rb");
  if (!file)
    return std::nullopt;
  Bytes bytes;
  std::uint8_t chunk[4096];
  for (std::size_t read; (read = std::fread(chunk, 1, sizeof chunk, file)) > 0;)
    bytes.insert(bytes.end(), chunk, chunk + read);
  std::fclose(file);
  return bytes;
}

} // namespace ScratchPadd::Storage
