// Messages.hpp - asking StoragePadd to save a file, and its answer.
//
//   sendTo(addressOf("StoragePadd"),
//          Storage::SaveFile{.path = "saves/slot1.bin", .bytes = std::make_shared<const
//          Storage::Bytes>(...)});
//   receive<Storage::FileSaved>([](const Storage::FileSaved &saved) { ... saved.ok ... });
#pragma once

#include <ScratchPadd/Storage/Files.hpp>

#include <memory>
#include <string>

namespace ScratchPadd::Storage {

// Saves `bytes` as `path`, under the app's files folder (Folders::files):
// whole, so a power-off midway leaves the old file (saveFile). A path that
// climbs out of the folder ("..") is refused.
struct SaveFile {
  std::string path;
  std::shared_ptr<const Bytes> bytes; // (shared: not copied on its way)
};

// The answer, to the padd that asked
struct FileSaved {
  std::string path; // as asked
  bool ok = false;
  std::string error; // why not
};

} // namespace ScratchPadd::Storage
