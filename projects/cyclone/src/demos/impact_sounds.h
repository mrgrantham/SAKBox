/*
 * SAKBox: sounds for the rigid-body demos, played with ScratchPadd's audio
 * engine (//projects/scratchpadd/audio).
 */
#pragma once

#include <ScratchPadd/Audio/Engine.hpp>
#include <cyclone/contacts.h>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * Knocks for the rigid-body demos: after each physics step, hear() looks at
 * its contacts for pairs of bodies that hit (rather than rest on) each other,
 * and at the end of the frame playHits() plays a knock for each pair heard. Louder the faster they
 * hit, lower the heavier the lighter of the two is, and panned to where it happened on screen. A
 * pair knocks at most once every kRepeatSeconds, and at most kHitsPerFrame pairs knock in a frame
 * (the hardest), so a collapsing stack doesn't make hundreds of clicks. Sounds are made in code
 * (ScratchPadd::Audio::MadeSounds).
 */
class ImpactSounds {
public:
  /** Sounds below this closing speed (m/s) are resting, not hitting */
  static constexpr float kQuietestHit = 1.0f;
  /** A hit this fast (m/s) or faster is as loud as a knock gets */
  static constexpr float kLoudestHit = 10.0f;
  static constexpr float kRepeatSeconds = 0.1f;
  static constexpr int kHitsPerFrame = 6;

  /** Plays through `output`; if it can't start, stays silent (see error()) */
  explicit ImpactSounds(ScratchPadd::Audio::Output &output);

  /**
   * Notes the hits among a step's `contacts`, `seconds` after the last step.
   * A frame can take several steps.
   */
  void hear(const cyclone::Contact *contacts, unsigned count, float seconds);

  /** Plays the hits heard since the last call: call once a frame */
  void playHits();

  /** A blast at `position` */
  void blast(const cyclone::Vector3 &position);

  /** Where the camera looks from: its angle around the vertical (degrees) */
  void setCameraAngle(float degrees) { cameraAngle_ = degrees; }

  /** Call every frame, paused or not: collects finished sounds */
  void update() { engine_.update(); }

  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }
  bool playing() const { return error_.empty(); }
  const std::string &error() const { return error_; }

  /**
   * Knocks played so far, and hits not played: quieter than every sound
   * playing while every voice was busy (the engine takes over the quietest
   * for a louder one)
   */
  std::uint32_t knocks() const { return knocks_; }
  std::uint32_t dropped() const { return dropped_; }

private:
  /** A hit: the fastest contact between a pair of bodies in a frame */
  struct Hit {
    std::pair<const void *, const void *> pair;
    float speed = 0;
    cyclone::Vector3 point;
    float mass = 1; // the lighter body's (kg)
  };

  float panFor(const cyclone::Vector3 &position) const;

  ScratchPadd::Audio::Engine engine_;
  std::string error_;
  bool enabled_ = true;
  float cameraAngle_ = 0;
  float clock_ = 0;
  std::uint32_t knocks_ = 0;
  std::uint32_t dropped_ = 0;
  std::uint32_t jitter_ = 1; // for small differences in pitch
  // When each pair of bodies last knocked
  std::map<std::pair<const void *, const void *>, float> lastKnock_;
  std::vector<Hit> heard_; // this frame's, until playHits()
};
