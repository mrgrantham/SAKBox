#include "impact_sounds.h"

void ImpactSounds::playHits() {
  hits_.takeKnocks(knocksToPlay_);
  if (!sounds_.enabled() || !sounds_.playing()) {
    return;
  }
  for (const Knock &knock : knocksToPlay_) {
    if (sounds_.play("knock", {.volume = knock.volume, .pan = knock.pan, .pitch = knock.pitch})) {
      ++knocks_;
    } else {
      ++dropped_;
    }
  }
}

void ImpactSounds::blast(const cyclone::Vector3 &position) {
  sounds_.play("blast", {.volume = 1.0f, .pan = hits_.panFor(position)});
}
