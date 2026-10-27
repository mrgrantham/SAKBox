/*
 * SAKBox: the rigid-body demos' physics clock. Also used by the Cyclone
 * padds example (//projects/examples/cyclone_padds), which changes the step
 * length while it runs.
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
  static constexpr double kDefaultStepSeconds = 1.0 / 60;
  static constexpr unsigned kMostSubSteps = 16;
  /**
   * The demos start with 4 sub-steps a step. They share the step's resolver
   * sweeps (cyclone::ContactResolver::setSweeps), so they cost about what
   * one does, and over 20 pushed ragdolls the most a joint stretched went
   * from 7.2 cm in whole steps to 0.004 cm at 60 steps a second (64 cm to
   * 0.3 cm at 20). See ragdoll_test. The engine itself (cyclone::World)
   * still defaults to whole steps.
   */
  static constexpr unsigned kDefaultSubSteps = 4;
  /**
   * A longer frame (a stall, a breakpoint) counts as this long: the
   * simulation slows down rather than taking ever more steps to catch up
   */
  static constexpr double kLongestFrame = 0.1;
  /**
   * A step is due this much of a step early, so frame times that wobble
   * around the step (16 and 17 ms, for 60 Hz) take one step each rather than
   * now and then none and then two
   */
  static constexpr double kEarliness = 0.25;

  /** Adds a frame `seconds` long; returns how many steps are due */
  unsigned stepsFor(double seconds) {
    pending_ += std::clamp(seconds, 0.0, kLongestFrame);
    const auto due = static_cast<unsigned>(std::floor(pending_ / stepSeconds_ + kEarliness));
    pending_ -= due * stepSeconds_;
    return due;
  }

  /**
   * How long each step is: kDefaultStepSeconds to start. Changing it keeps
   * the time not yet stepped.
   */
  void setStepSeconds(double seconds) { stepSeconds_ = seconds; }
  double stepSeconds() const { return stepSeconds_; }

  /** Sub-steps per step: 1 (whole steps) to kMostSubSteps */
  void setSubSteps(unsigned count) { subSteps_ = std::clamp(count, 1u, kMostSubSteps); }
  unsigned subSteps() const { return subSteps_; }
  double subStepSeconds() const { return stepSeconds_ / subSteps_; }

private:
  double stepSeconds_ = kDefaultStepSeconds;
  double pending_ = 0; // seconds not yet stepped (negative when a step came early)
  unsigned subSteps_ = kDefaultSubSteps;
};
