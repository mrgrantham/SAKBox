#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace ScratchPadd::Audio {

// A fixed-size queue for exactly one producer thread and one consumer
// thread. Both push() and pop() are wait-free: they never lock, allocate or
// wait for the other thread, so the audio thread can use them (Doumler,
// "Lock-free programming with modern C++", ACCU 2017). The memory is
// allocated once, by the constructor. T must be trivially copyable, so
// moving one in or out can't allocate either.
template <typename T> class SpscQueue {
  static_assert(std::is_trivially_copyable_v<T>, "SpscQueue holds trivially copyable values");

public:
  // Holds up to `capacity` items
  explicit SpscQueue(std::uint32_t capacity) : slots_(capacity + 1) {}

  SpscQueue(const SpscQueue &) = delete;
  SpscQueue &operator=(const SpscQueue &) = delete;

  // Producer only. Returns false, changing nothing, if the queue is full.
  bool push(const T &item) {
    const std::uint32_t tail = tail_.load(std::memory_order_relaxed);
    const std::uint32_t next = advance(tail);
    if (next == head_.load(std::memory_order_acquire)) {
      return false;
    }
    slots_[tail] = item;
    tail_.store(next, std::memory_order_release);
    return true;
  }

  // Consumer only. Returns false if the queue is empty.
  bool pop(T &item) {
    const std::uint32_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    item = slots_[head];
    head_.store(advance(head), std::memory_order_release);
    return true;
  }

  std::uint32_t capacity() const { return static_cast<std::uint32_t>(slots_.size() - 1); }

private:
  std::uint32_t advance(std::uint32_t index) const {
    return index + 1 == slots_.size() ? 0 : index + 1;
  }

  // One slot always stays empty, so a full queue and an empty one differ
  std::vector<T> slots_;
  // The next slot to read (written by the consumer) and to write (by the
  // producer), on separate cache lines so the threads don't contend
  alignas(64) std::atomic<std::uint32_t> head_{0};
  alignas(64) std::atomic<std::uint32_t> tail_{0};
};

// A fixed-size ring of samples for one writer thread and one reader thread,
// copied in and out in blocks: how music decoded ahead on the owner's
// thread reaches the audio thread. Wait-free, like SpscQueue.
class SampleRing {
public:
  explicit SampleRing(std::uint32_t capacity) : samples_(capacity + 1) {}

  SampleRing(const SampleRing &) = delete;
  SampleRing &operator=(const SampleRing &) = delete;

  // Writer only. Copies as many of `count` samples as fit; returns how many.
  std::uint32_t write(const float *from, std::uint32_t count) {
    const std::uint32_t tail = tail_.load(std::memory_order_relaxed);
    const std::uint32_t head = head_.load(std::memory_order_acquire);
    const std::uint32_t space = freeSpace(head, tail);
    const std::uint32_t n = std::min(count, space);
    const std::uint32_t size = static_cast<std::uint32_t>(samples_.size());
    const std::uint32_t first = std::min(n, size - tail);
    std::copy_n(from, first, samples_.begin() + tail);
    std::copy_n(from + first, n - first, samples_.begin());
    tail_.store((tail + n) % size, std::memory_order_release);
    return n;
  }

  // Reader only. Copies up to `count` samples out; returns how many.
  std::uint32_t read(float *to, std::uint32_t count) {
    const std::uint32_t head = head_.load(std::memory_order_relaxed);
    const std::uint32_t tail = tail_.load(std::memory_order_acquire);
    const std::uint32_t size = static_cast<std::uint32_t>(samples_.size());
    const std::uint32_t available = (tail + size - head) % size;
    const std::uint32_t n = std::min(count, available);
    const std::uint32_t first = std::min(n, size - head);
    std::copy_n(samples_.begin() + head, first, to);
    std::copy_n(samples_.begin(), n - first, to + first);
    head_.store((head + n) % size, std::memory_order_release);
    return n;
  }

  // How many samples are waiting to be read (either thread; a snapshot)
  std::uint32_t available() const {
    const std::uint32_t size = static_cast<std::uint32_t>(samples_.size());
    return (tail_.load(std::memory_order_acquire) + size - head_.load(std::memory_order_acquire)) %
           size;
  }

  std::uint32_t capacity() const { return static_cast<std::uint32_t>(samples_.size() - 1); }

private:
  std::uint32_t freeSpace(std::uint32_t head, std::uint32_t tail) const {
    const std::uint32_t size = static_cast<std::uint32_t>(samples_.size());
    return (head + size - tail - 1) % size;
  }

  std::vector<float> samples_;
  alignas(64) std::atomic<std::uint32_t> head_{0};
  alignas(64) std::atomic<std::uint32_t> tail_{0};
};

} // namespace ScratchPadd::Audio
