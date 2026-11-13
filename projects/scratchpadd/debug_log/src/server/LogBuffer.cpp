#include <ScratchPadd/DebugLog/LogBuffer.hpp>

namespace ScratchPadd::DebugLog {

void LogBuffer::append(std::string_view text) {
  {
    std::lock_guard lock(mutex_);
    text_.append(text);
    if (text_.size() > capacity_) {
      // A quarter dropped at a time (not a little every line), at a line's
      // start
      std::size_t cut = text_.size() - capacity_ + capacity_ / 4;
      const std::size_t lineEnd = text_.find('\n', cut);
      cut = lineEnd == std::string::npos ? text_.size() : lineEnd + 1;
      text_.erase(0, cut);
      dropped_ += cut;
    }
  }
  appended_.notify_all();
}

std::string LogBuffer::readFrom(std::uint64_t &position, std::chrono::milliseconds wait) {
  std::unique_lock lock(mutex_);
  const auto hasNew = [&] { return position < dropped_ + text_.size(); };
  if (!hasNew() && wait.count() > 0)
    appended_.wait_for(lock, wait, hasNew);
  if (position < dropped_)
    position = dropped_; // gone: from the oldest kept
  std::string text = text_.substr(static_cast<std::size_t>(position - dropped_));
  position = dropped_ + text_.size();
  return text;
}

std::uint64_t LogBuffer::end() const {
  std::lock_guard lock(mutex_);
  return dropped_ + text_.size();
}

} // namespace ScratchPadd::DebugLog
