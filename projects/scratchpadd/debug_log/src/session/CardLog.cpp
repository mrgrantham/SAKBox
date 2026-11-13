#include <ScratchPadd/DebugLog/CardLog.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <utility>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ScratchPadd::DebugLog {

namespace {

std::atomic<CardLog *> activeLog{nullptr};

// The file's cache written out to the card (or disk)
void sync(std::FILE *file) {
  std::fflush(file);
#if defined(_WIN32)
  _commit(_fileno(file));
#else
  ::fsync(::fileno(file));
#endif
}

double milliseconds(std::chrono::steady_clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

} // namespace

std::unique_ptr<CardLog> CardLog::open(const std::string &path, unsigned writeEvery,
                                       LineFormatter formatLine, std::string &error) {
  std::FILE *file = std::fopen(path.c_str(), "w");
  if (!file) {
    error = "can't write " + path;
    return nullptr;
  }
  return std::unique_ptr<CardLog>(new CardLog(file, writeEvery, std::move(formatLine)));
}

CardLog::CardLog(std::FILE *file, unsigned writeEvery, LineFormatter formatLine)
    : writeEvery_(std::max(1u, writeEvery)), file_(file), formatLine_(std::move(formatLine)) {}

CardLog::~CardLog() {
  if (active() == this) {
    setActive(nullptr);
  }
  std::lock_guard fileLock(fileMutex_);
  writeWaiting();
  if (syncs_ % kSyncsPerReport != 0) {
    report();
  }
  std::fclose(file_);
}

void CardLog::add(std::string_view line, bool urgent) {
  bool writeNow = false;
  {
    std::lock_guard lock(mutex_);
    waiting_.append(line);
    ++linesWaiting_;
    if (linesWaiting_ < writeEvery_ && !urgent) {
      return;
    }
    if (!ringWriter_) {
      writeNow = true; // (no writer: this thread writes)
    } else if (!rung_) {
      rung_ = true;
      ringWriter_(); // (under the lock, so detach() waits for it)
    }
  }
  if (writeNow) {
    write();
  }
}

void CardLog::attach(std::function<void()> ringWriter) {
  std::lock_guard lock(mutex_);
  ringWriter_ = std::move(ringWriter);
  rung_ = false;
}

void CardLog::detach() {
  std::lock_guard lock(mutex_);
  ringWriter_ = nullptr;
}

void CardLog::write() {
  std::lock_guard fileLock(fileMutex_);
  writeWaiting();
}

void CardLog::writeWaiting() {
  std::string lines;
  {
    std::lock_guard lock(mutex_);
    lines.swap(waiting_);
    linesWaiting_ = 0;
    rung_ = false;
  }
  if (lines.empty()) {
    return;
  }
  std::fwrite(lines.data(), 1, lines.size(), file_);
  const Clock::time_point started = Clock::now();
  sync(file_);
  const Clock::duration took = Clock::now() - started;
  ++syncs_;
  totalSync_ += took;
  worstSync_ = std::max(worstSync_, took);
  if (syncs_ % kSyncsPerReport == 0) {
    report();
  }
}

void CardLog::report() {
  if (syncs_ == 0) {
    return;
  }
  char text[128];
  std::snprintf(text, sizeof text, "%u card syncs so far: %.1f ms each, %.1f ms at worst",
                static_cast<unsigned>(syncs_), milliseconds(totalSync_) / syncs_,
                milliseconds(worstSync_));
  const std::string line = formatLine_ ? formatLine_(text) : std::string(text) + "\n";
  std::fwrite(line.data(), 1, line.size(), file_);
  sync(file_);
}

unsigned CardLog::writeEvery() const {
  std::lock_guard lock(mutex_);
  return writeEvery_;
}

void CardLog::setWriteEvery(unsigned lines) {
  std::lock_guard lock(mutex_);
  writeEvery_ = std::max(1u, lines);
}

CardLog::Figures CardLog::figures() const {
  Figures figures;
  {
    std::lock_guard fileLock(fileMutex_);
    figures.syncs = syncs_;
    figures.meanMilliseconds =
        syncs_ ? static_cast<float>(milliseconds(totalSync_) / syncs_) : 0.0f;
    figures.worstMilliseconds = static_cast<float>(milliseconds(worstSync_));
  }
  std::lock_guard lock(mutex_);
  figures.linesWaiting = linesWaiting_;
  return figures;
}

CardLog *CardLog::active() { return activeLog.load(); }
void CardLog::setActive(CardLog *log) { activeLog = log; }

} // namespace ScratchPadd::DebugLog
