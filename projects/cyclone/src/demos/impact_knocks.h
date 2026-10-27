/*
 * SAKBox: which hits in a rigid-body scene to knock for, and how loud, how
 * high and where. No audio dependency: ImpactSounds plays the knocks with
 * ScratchPadd's audio engine in the demos, and the Cyclone padds example
 * (//projects/examples/cyclone_padds) sends them to AudioPadd.
 */
#pragma once

#include <cyclone/contacts.h>

#include <cstdint>
#include <map>
#include <span>
#include <utility>
#include <vector>

/** How to play one knock (as ScratchPadd::Audio::VoiceSettings takes it) */
struct Knock {
  float volume = 1; // 0.15 for the lightest hit heard, to 1
  float pan = 0;    // -0.8 (left) to 0.8 (right)
  float pitch = 1;  // 1 for a 1 kg body, lower for heavier
};

/**
 * After each physics step, hear() looks at its contacts for pairs of bodies
 * that hit (rather than rest on) each other; once a frame, takeKnocks()
 * gives a knock for each pair heard. Louder the faster they hit, lower the
 * heavier the lighter of the two is, and panned to where it happened on
 * screen. A pair knocks at most once every kRepeatSeconds, and at most
 * kHitsPerFrame pairs knock in a frame (the hardest), so a collapsing stack
 * doesn't make hundreds of clicks.
 */
class ImpactKnocks {
public:
  /** Hits below this closing speed (m/s) are resting, not hitting */
  static constexpr float kQuietestHit = 1.0f;
  /** A hit this fast (m/s) or faster is as loud as a knock gets */
  static constexpr float kLoudestHit = 10.0f;
  static constexpr float kRepeatSeconds = 0.1f;
  static constexpr int kHitsPerFrame = 6;

  /**
   * Notes the hits among a step's `contacts`, `seconds` after the last step.
   * A frame can take several steps.
   */
  void hear(std::span<const cyclone::Contact> contacts, float seconds);

  /**
   * The knocks for the hits heard since the last call, the hardest first,
   * into `knocks` (cleared first). Call once a frame.
   */
  void takeKnocks(std::vector<Knock> &knocks);

  /**
   * Where the camera looks from: its angle around the vertical (degrees),
   * looking at the origin from +x when 0, as the demos' camera does
   */
  void setCameraAngle(float degrees) { cameraAngle_ = degrees; }

  /** The pan for a sound at `position`: where it is on screen */
  float panFor(const cyclone::Vector3 &position) const;

private:
  /** A hit: the fastest contact between a pair of bodies in a frame */
  struct Hit {
    std::pair<const void *, const void *> pair;
    float speed = 0;
    cyclone::Vector3 point;
    float mass = 1; // the lighter body's (kg)
  };

  float cameraAngle_ = 0;
  float clock_ = 0;
  std::uint32_t jitter_ = 1; // for small differences in pitch
  // When each pair of bodies last knocked
  std::map<std::pair<const void *, const void *>, float> lastKnock_;
  std::vector<Hit> heard_; // this frame's, until takeKnocks()
};
