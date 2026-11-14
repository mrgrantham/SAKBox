// Files.hpp - a whole file saved or loaded at once. Saving writes
// `path.tmp`, syncs it to the card or disk, and only then puts it in place
// of `path`, so a power-off midway leaves the old file (or, on a console,
// where the old one is removed first, the whole new one as path.tmp, which
// loadFile() takes when path is gone). StoragePadd saves this way on its
// thread (SaveFile); anyone may load.
#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ScratchPadd::Storage {

using Bytes = std::vector<std::uint8_t>;

// Saves `bytes` as `path` (its folder made as needed). False, with `error`
// saying why, if it couldn't.
bool saveFile(const std::string &path, std::span<const std::uint8_t> bytes, std::string &error);

// The file at `path` (or a save that didn't finish being put in place), or
// nothing if there's neither
std::optional<Bytes> loadFile(const std::string &path);

// A file's buffered writes written out to the card or disk (fflush only
// reaches libfat's cache on a DS, which reaches the card when the file
// closes)
void sync(std::FILE *file);

} // namespace ScratchPadd::Storage
