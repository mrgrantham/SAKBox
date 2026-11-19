// FrameProfile.hpp - where a frame's time goes (Gregory, Game Engine
// Architecture 10.8, "In-Game Profiling": named blocks timed every frame,
// averaged, shown on the screen while the game runs). Measure before
// optimising: the DS's frames came in fits and starts.
//
// Each frame, time its sections; once a second, line() says, for that
// second, the frames drawn and each section's average and worst
// milliseconds a frame. What isn't in any section (other threads, waiting)
// is the rest of the frame's period.
//
//   ScratchPadd::FrameProfile profile({"dice", "draw", "wait"});
//   profile.frameStarted();
//   { auto timing = profile.time(0); ... the dice ... }
//   ... later: if (profile.secondDone()) show(profile.line());
//
// Cheap: two reads of the steady clock a section. Not thread-safe (one
// profile, one thread).
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace ScratchPadd {

class FrameProfile {
public:
  static constexpr std::size_t kMostSections = 10;
  using Clock = std::chrono::steady_clock;

  // The sections' short names (at most kMostSections; more are ignored)
  FrameProfile(std::initializer_list<const char *> names) {
    for (const char *name : names) {
      if (count_ == kMostSections)
        break;
      sections_[count_++].name = name;
    }
  }

  // A frame begins (call once a frame, before its sections)
  void frameStarted() {
    const Clock::time_point now = Clock::now();
    if (!started_) {
      started_ = true;
      secondStart_ = now;
      return;
    }
    ++frames_;
    if (now - secondStart_ >= std::chrono::seconds(1)) {
      makeLine(std::chrono::duration<double>(now - secondStart_).count());
      secondStart_ = now;
      frames_ = 0;
      for (std::size_t i = 0; i < count_; ++i)
        sections_[i].total = sections_[i].worst = Clock::duration::zero();
      fresh_ = true;
    }
  }

  // Times section `index` until the returned object goes
  class Timing {
  public:
    Timing(FrameProfile &profile, std::size_t index)
        : profile_(profile), index_(index), start_(Clock::now()) {}
    ~Timing() { profile_.add(index_, Clock::now() - start_); }
    Timing(const Timing &) = delete;
    Timing &operator=(const Timing &) = delete;

  private:
    FrameProfile &profile_;
    std::size_t index_;
    Clock::time_point start_;
  };
  Timing time(std::size_t index) { return Timing(*this, index); }

  // Adds `spent` to section `index` this frame
  void add(std::size_t index, Clock::duration spent) {
    if (index >= count_)
      return;
    Section &section = sections_[index];
    section.total += spent;
    section.frame += spent;
    section.worst = std::max(section.worst, section.frame);
  }
  // Ends the frame's per-section sums (call at the end of each frame, so a
  // section timed twice in a frame counts once toward its worst)
  void frameEnded() {
    for (std::size_t i = 0; i < count_; ++i)
      sections_[i].frame = Clock::duration::zero();
  }

  // Whether a new line is ready since the last ask (once a second)
  bool secondDone() {
    const bool was = fresh_;
    fresh_ = false;
    return was;
  }
  // "41fps dice 3.1/9.0 draw 12.0/15.2 ...": frames a second, then each
  // section's average/worst milliseconds a frame over the last second
  const std::string &line() const { return line_; }

private:
  struct Section {
    const char *name = "";
    Clock::duration total{}, worst{}, frame{};
  };

  void makeLine(double seconds) {
    char text[256];
    int used = std::snprintf(text, sizeof text, "%.0ffps", static_cast<double>(frames_) / seconds);
    for (std::size_t i = 0; i < count_ && used > 0 && used < static_cast<int>(sizeof text); ++i) {
      const auto ms = [](Clock::duration d) {
        return std::chrono::duration<double, std::milli>(d).count();
      };
      const double average = frames_ > 0 ? ms(sections_[i].total) / frames_ : 0;
      used += std::snprintf(text + used, sizeof text - static_cast<std::size_t>(used),
                            " %s %.1f/%.0f", sections_[i].name, average, ms(sections_[i].worst));
    }
    line_ = text;
  }

  std::array<Section, kMostSections> sections_{};
  std::size_t count_ = 0;
  bool started_ = false, fresh_ = false;
  Clock::time_point secondStart_{};
  int frames_ = 0;
  std::string line_;
};

} // namespace ScratchPadd
