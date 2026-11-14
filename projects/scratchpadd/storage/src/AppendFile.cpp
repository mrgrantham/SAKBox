#include <ScratchPadd/Storage/AppendFile.hpp>
#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace ScratchPadd::Storage {

namespace {

double milliseconds(std::chrono::steady_clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

} // namespace

std::string AppendFile::runPath(const std::string &path, unsigned run) {
  const auto slash = path.find_last_of("/\\");
  const auto dot = path.find_last_of('.');
  const bool hasExtension = dot != std::string::npos && (slash == std::string::npos || dot > slash);
  const std::string stem = hasExtension ? path.substr(0, dot) : path;
  const std::string extension = hasExtension ? path.substr(dot) : "";
  return stem + "." + std::to_string(run) + extension;
}

std::unique_ptr<AppendFile> AppendFile::open(const std::string &path, Options options,
                                             std::string &error) {
  if (!ready(error))
    return nullptr;
  const auto slash = path.find_last_of("/\\");
  if (slash != std::string::npos && !makeFolder(path.substr(0, slash + 1))) {
    error = "can't make the folder for " + path;
    return nullptr;
  }
  // The runs before, one further back each, the oldest dropped
  if (options.keepRuns > 0) {
    std::remove(runPath(path, options.keepRuns).c_str());
    for (unsigned run = options.keepRuns; run > 1; --run)
      std::rename(runPath(path, run - 1).c_str(), runPath(path, run).c_str());
    std::rename(path.c_str(), runPath(path, 1).c_str());
  }
  std::FILE *file = std::fopen(path.c_str(), "w");
  if (!file) {
    error = "can't write " + path;
    return nullptr;
  }
  return std::unique_ptr<AppendFile>(new AppendFile(file, path, std::move(options)));
}

AppendFile::AppendFile(std::FILE *file, std::string path, Options options)
    : writeEvery_(std::max(1u, options.writeEvery)), file_(file), path_(std::move(path)),
      name_(std::move(options.name)), formatLine_(std::move(options.formatLine)) {}

AppendFile::~AppendFile() {
  detach();
  std::lock_guard fileLock(fileMutex_);
  writeWaiting(false);
  if (syncs_ % kSyncsPerReport != 0)
    report();
  std::fclose(file_);
}

void AppendFile::add(std::string_view piece, bool urgent) {
  bool writeNow = false;
  {
    std::lock_guard lock(mutex_);
    const Clock::time_point now = Clock::now();
    if (piecesWaiting_ == 0)
      oldestWaiting_ = now;
    waiting_.append(piece);
    ++piecesWaiting_;
    if (urgent || now - oldestWaiting_ >= kLongestWait) {
      writeNow = true; // (it can't wait for StoragePadd: this thread writes)
    } else if (piecesWaiting_ >= writeEvery_) {
      if (!ring_) {
        writeNow = true; // (no StoragePadd: this thread writes)
      } else if (!rung_) {
        rung_ = true;
        rungAt_ = now;
        ring_(); // (under the lock, so detach() waits for it)
      }
    }
  }
  if (writeNow) {
    std::lock_guard fileLock(fileMutex_);
    writeWaiting(true);
  }
}

void AppendFile::attach(std::function<void()> ring) {
  std::lock_guard lock(mutex_);
  ring_ = std::move(ring);
  rung_ = false;
}

void AppendFile::detach() {
  std::lock_guard lock(mutex_);
  ring_ = nullptr;
}

void AppendFile::write() {
  std::lock_guard fileLock(fileMutex_);
  writeWaiting(false);
}

void AppendFile::writeWaiting(bool byAdder) {
  std::string pieces;
  bool attached = false;
  {
    std::lock_guard lock(mutex_);
    pieces.swap(waiting_);
    piecesWaiting_ = 0;
    attached = static_cast<bool>(ring_);
    if (rung_) {
      if (!byAdder)
        worstWait_ = std::max(worstWait_, Clock::now() - rungAt_);
      rung_ = false;
    }
  }
  if (pieces.empty())
    return;
  std::fwrite(pieces.data(), 1, pieces.size(), file_);
  const Clock::time_point started = Clock::now();
  sync(file_);
  const Clock::duration took = Clock::now() - started;
  ++syncs_;
  if (byAdder && attached)
    ++writtenByAdder_; // (urgent, or waited too long for StoragePadd)
  totalSync_ += took;
  worstSync_ = std::max(worstSync_, took);
  if (syncs_ % kSyncsPerReport == 0)
    report();
}

std::string AppendFile::figuresLine() const {
  const Figures f = figures();
  char text[160];
  std::snprintf(
      text, sizeof text,
      "%u syncs: %.1f ms each, %.1f at worst; waited %.0f ms at worst; %u written at once",
      static_cast<unsigned>(f.syncs), f.meanSyncMilliseconds, f.worstSyncMilliseconds,
      f.worstWaitMilliseconds, static_cast<unsigned>(f.written));
  return text;
}

void AppendFile::report() {
  if (syncs_ == 0)
    return;
  char text[200];
  std::snprintf(text, sizeof text,
                "%u syncs so far: %.1f ms each, %.1f ms at worst; StoragePadd's writes waited "
                "%.0f ms at worst to run; %u written at once (urgent, or waited too long)",
                static_cast<unsigned>(syncs_), milliseconds(totalSync_) / syncs_,
                milliseconds(worstSync_), milliseconds(worstWait_),
                static_cast<unsigned>(writtenByAdder_));
  const std::string line = formatLine_ ? formatLine_(text) : std::string(text) + "\n";
  std::fwrite(line.data(), 1, line.size(), file_);
  sync(file_);
}

std::optional<unsigned> AppendFile::writeEvery() const {
  std::lock_guard lock(mutex_);
  return writeEvery_;
}

void AppendFile::setWriteEvery(unsigned pieces) {
  std::lock_guard lock(mutex_);
  writeEvery_ = std::max(1u, pieces);
}

AppendFile::Figures AppendFile::figures() const {
  Figures figures;
  {
    std::lock_guard fileLock(fileMutex_);
    figures.syncs = syncs_;
    figures.meanSyncMilliseconds =
        syncs_ ? static_cast<float>(milliseconds(totalSync_) / syncs_) : 0.0f;
    figures.worstSyncMilliseconds = static_cast<float>(milliseconds(worstSync_));
    figures.worstWaitMilliseconds = static_cast<float>(milliseconds(worstWait_));
    figures.written = writtenByAdder_;
  }
  std::lock_guard lock(mutex_);
  figures.waiting = piecesWaiting_;
  return figures;
}

} // namespace ScratchPadd::Storage
