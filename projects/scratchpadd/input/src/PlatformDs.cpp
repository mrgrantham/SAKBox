// The DS's buttons and touch screen
#if defined(__NDS__)
#include <ScratchPadd/Input/Devices.hpp>

#include <nds.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace ScratchPadd::Input {

namespace {

// The buttons, sampled at every screen refresh (60 a second) by a thread of
// their own, ahead of the others, counting each press and release. GamePadd
// reads on the main thread, which can be late when it's busy; counted here,
// a tap between two of its reads is still seen. (Calico's way to act on an
// interrupt: a thread that waits for it, as an interrupt handler mustn't
// call the C library or lock.)
class ButtonSampler {
public:
  static ButtonSampler &instance() {
    static ButtonSampler sampler;
    return sampler;
  }
  void copyCounts(std::array<std::uint16_t, kButtons> &presses,
                  std::array<std::uint16_t, kButtons> &releases) {
    std::lock_guard lock(mutex_);
    presses = presses_;
    releases = releases_;
  }

private:
  ButtonSampler() {
    // Each of the DS's key bits, as the Button it is
    for (std::size_t bit = 0; bit < buttonOfBit_.size(); ++bit) {
      const std::uint32_t buttons = fromNintendo(Kind::Ds, 1u << bit, 0, 0, 0, 0, {}).held;
      buttonOfBit_[bit] = buttons ? static_cast<int>(__builtin_ctz(buttons)) : -1;
    }
    // (kept, not detached: on a DS, std::thread::detach() ends the app;
    // melonDS powered off and a DS Lite hung, 2026-11-13)
    thread_ = std::thread([this] {
      // Ahead of everything else the app runs, padds and their timers
      // included (main - 1): it takes microseconds, 60 times a second
      threadSetPrio(threadGetSelf(), MAIN_THREAD_PRIO - 2);
      while (!stopping_) {
        threadIrqWait(true, IRQ_VBLANK);
        sample();
      }
    });
  }
  // (when the app ends: within a frame)
  ~ButtonSampler() {
    stopping_ = true;
    thread_.join();
  }
  ButtonSampler(const ButtonSampler &) = delete;
  ButtonSampler &operator=(const ButtonSampler &) = delete;

  void sample() {
    const unsigned held = keypadGetState();
    const unsigned changed = held ^ lastHeld_;
    lastHeld_ = held;
    if (!changed)
      return;
    std::lock_guard lock(mutex_);
    for (std::size_t bit = 0; bit < buttonOfBit_.size(); ++bit) {
      if (!(changed >> bit & 1) || buttonOfBit_[bit] < 0)
        continue;
      auto &counts = held >> bit & 1 ? presses_ : releases_;
      ++counts[static_cast<std::size_t>(buttonOfBit_[bit])];
    }
  }

  std::atomic<bool> stopping_{false};
  std::thread thread_;
  std::array<int, 16> buttonOfBit_{};
  unsigned lastHeld_ = 0; // (the sampler's thread only)
  std::mutex mutex_;
  std::array<std::uint16_t, kButtons> presses_{}, releases_{};
};

} // namespace

bool readPlatformPresses(std::array<std::uint16_t, kButtons> &presses,
                         std::array<std::uint16_t, kButtons> &releases) {
  ButtonSampler::instance().copyCounts(presses, releases);
  return true;
}

static_assert(NintendoKeys::kA == KEY_A && NintendoKeys::kB == KEY_B && NintendoKeys::kX == KEY_X &&
              NintendoKeys::kY == KEY_Y && NintendoKeys::kL == KEY_L && NintendoKeys::kR == KEY_R &&
              NintendoKeys::kStart == KEY_START && NintendoKeys::kSelect == KEY_SELECT &&
              NintendoKeys::kUp == KEY_UP && NintendoKeys::kDown == KEY_DOWN &&
              NintendoKeys::kLeft == KEY_LEFT && NintendoKeys::kRight == KEY_RIGHT);

Gamepad readPlatformPad() {
  scanKeys();
  const std::uint32_t held = keysHeld();
  Touch touch;
  if (held & KEY_TOUCH) {
    touchPosition at{};
    touchRead(&at);
    touch = {true, static_cast<std::int16_t>(at.px), static_cast<std::int16_t>(at.py)};
  }
  return fromNintendo(Kind::Ds, held, 0, 0, 0, 0, touch);
}

} // namespace ScratchPadd::Input
#endif
