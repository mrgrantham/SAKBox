/*
 * SAKBox: the rigid-body demos' physics clock.
 */
#pragma once

#include <algorithm>
#include <cmath>

/**
 * Runs physics in fixed steps however long frames take: each frame adds its
 * time, and the steps it covers are due. So the simulation is the same at any
 * frame rate (frames used to step by their own length, so a 144 Hz screen or
 * a stutter changed it). Each step can be split into sub-steps, smaller
 * steps that stretch joints less on impact and take less energy out of a
 * swing (see cyclone::World::setSubSteps).
 */
class FixedSteps {
public:
  static constexpr double kStepSeconds = 1.0 / 60;
  static constexpr unsigned kMostSubSteps = 16;
  /**
   * A longer frame (a stall, a breakpoint) counts as this long: the
   * simulation slows down rather than taking ever more steps to catch up
   */
  static constexpr double kLongestFrame = 0.1;
  /**
   * A step is due this much early, so frame times that wobble around the
   * step (16 and 17 ms, for 60 Hz) take one step each rather than now and
   * then none and then two
   */
  static constexpr double kEarliness = 0.25 * kStepSeconds;

  /** Adds a frame `seconds` long; returns how many steps are due */
  unsigned stepsFor(double seconds) {
    pending_ += std::clamp(seconds, 0.0, kLongestFrame);
    const auto due = static_cast<unsigned>(std::floor((pending_ + kEarliness) / kStepSeconds));
    pending_ -= due * kStepSeconds;
    return due;
  }

  /** Sub-steps per step: 1 (whole steps) to kMostSubSteps */
  void setSubSteps(unsigned count) { subSteps_ = std::clamp(count, 1u, kMostSubSteps); }
  unsigned subSteps() const { return subSteps_; }
  double subStepSeconds() const { return kStepSeconds / subSteps_; }

private:
  double pending_ = 0; // seconds not yet stepped (negative when a step came early)
  unsigned subSteps_ = 1;
};
