/*
 * SAKBox: sounds for the rigid-body demos, played with ScratchPadd's audio
 * engine (//projects/scratchpadd/audio).
 */
#pragma once

#include "impact_knocks.h"

#include <ScratchPadd/Audio/Engine.hpp>
#include <cyclone/contacts.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

/**
 * Knocks for the rigid-body demos, through the audio engine: ImpactKnocks
 * chooses the hits (see it for how), and this plays them. Sounds are made
 * in code (ScratchPadd::Audio::MadeSounds).
 */
class ImpactSounds {
public:
  /** Plays through `output`; if it can't start, stays silent (see error()) */
  explicit ImpactSounds(ScratchPadd::Audio::Output &output);

  /**
   * Notes the hits among a step's `contacts`, `seconds` after the last step.
   * A frame can take several steps.
   */
  void hear(std::span<const cyclone::Contact> contacts, float seconds) {
    hits_.hear(contacts, seconds);
  }

  /** Plays the hits heard since the last call: call once a frame */
  void playHits();

  /** A blast at `position` */
  void blast(const cyclone::Vector3 &position);

  /** Where the camera looks from: its angle around the vertical (degrees) */
  void setCameraAngle(float degrees) { hits_.setCameraAngle(degrees); }

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
  ScratchPadd::Audio::Engine engine_;
  ImpactKnocks hits_;
  std::vector<Knock> knocksToPlay_; // reused each frame
  std::string error_;
  bool enabled_ = true;
  std::uint32_t knocks_ = 0;
  std::uint32_t dropped_ = 0;
};
