/*
 * SAKBox: knocks for the demos, played with the demos' sounds (DemoSounds).
 */
#pragma once

#include "demo_sounds.h"
#include "impact_knocks.h"

#include <cyclone/contacts.h>

#include <cstdint>
#include <span>
#include <vector>

/**
 * Knocks for the demos: ImpactKnocks chooses the hits among bodies' or
 * particles' contacts (see it for how), and this plays them through a
 * demo's DemoSounds.
 */
class ImpactSounds {
public:
  explicit ImpactSounds(DemoSounds &sounds) : sounds_(sounds) {}

  /**
   * Notes the hits among a step's `contacts`, `seconds` after the last step.
   * A frame can take several steps.
   */
  void hear(std::span<const cyclone::Contact> contacts, float seconds) {
    hits_.hear(contacts, seconds);
  }
  void hear(std::span<const cyclone::ParticleContact> contacts, float seconds) {
    hits_.hear(contacts, seconds);
  }

  /** Plays the hits heard since the last call: call once a frame */
  void playHits();

  /** A blast at `position` */
  void blast(const cyclone::Vector3 &position);

  /** Where the camera looks from: its angle around the vertical (degrees) */
  void setCameraAngle(float degrees) { hits_.setCameraAngle(degrees); }

  /**
   * Knocks played so far, and hits not played: quieter than every sound
   * playing while every voice was busy (the engine takes over the quietest
   * for a louder one). Hits while sound is off count as neither.
   */
  std::uint32_t knocks() const { return knocks_; }
  std::uint32_t dropped() const { return dropped_; }

private:
  DemoSounds &sounds_;
  ImpactKnocks hits_;
  std::vector<Knock> knocksToPlay_; // reused each frame
  std::uint32_t knocks_ = 0;
  std::uint32_t dropped_ = 0;
};
