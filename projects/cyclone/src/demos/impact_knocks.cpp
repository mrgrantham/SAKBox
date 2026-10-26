#include "impact_knocks.h"

#include <algorithm>
#include <cmath>

namespace {

/** The velocity of `body`'s surface at `point` (still, for none: the ground) */
cyclone::Vector3 velocityAt(const cyclone::RigidBody *body, const cyclone::Vector3 &point) {
  if (!body) {
    return cyclone::Vector3(0, 0, 0);
  }
  return body->getVelocity() + body->getRotation() % (point - body->getPosition());
}

} // namespace

void ImpactKnocks::hear(const cyclone::Contact *contacts, unsigned count, float seconds) {
  clock_ += seconds;
  // The fastest contact for each pair of bodies (a box resting on the
  // ground has several contacts; it's one knock), over the frame's steps
  std::vector<Hit> &hits = heard_;
  for (unsigned i = 0; i < count; ++i) {
    const cyclone::Contact &contact = contacts[i];
    // Cyclone's closing velocity: body 0's relative to body 1, along the
    // normal, negative as they come together
    const cyclone::Vector3 relative = velocityAt(contact.body[0], contact.contactPoint) -
                                      velocityAt(contact.body[1], contact.contactPoint);
    const float speed = static_cast<float>(-(relative * contact.contactNormal));
    if (speed < kQuietestHit) {
      continue;
    }
    const auto pair = std::make_pair<const void *, const void *>(contact.body[0], contact.body[1]);
    auto hit = std::find_if(hits.begin(), hits.end(), [&](const Hit &h) { return h.pair == pair; });
    if (hit == hits.end()) {
      float mass = static_cast<float>(contact.body[0]->getMass());
      if (contact.body[1] && contact.body[1]->hasFiniteMass()) {
        mass = std::min(mass, static_cast<float>(contact.body[1]->getMass()));
      }
      hits.push_back({.pair = pair, .speed = speed, .point = contact.contactPoint, .mass = mass});
    } else if (speed > hit->speed) {
      hit->speed = speed;
      hit->point = contact.contactPoint;
    }
  }
}

void ImpactKnocks::takeKnocks(std::vector<Knock> &knocks) {
  knocks.clear();
  std::vector<Hit> &hits = heard_;
  // The hardest hits first, a few a frame, each pair not too often
  std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.speed > b.speed; });
  for (const Hit &hit : hits) {
    if (static_cast<int>(knocks.size()) == kHitsPerFrame) {
      break;
    }
    auto last = lastKnock_.find(hit.pair);
    if (last != lastKnock_.end() && clock_ - last->second < kRepeatSeconds) {
      continue;
    }
    const float loudness =
        std::min(1.0f, (hit.speed - kQuietestHit) / (kLoudestHit - kQuietestHit));
    // A little different each time: within 6% either way
    jitter_ = jitter_ * 1664525u + 1013904223u;
    const float jitter = 0.94f + 0.12f * static_cast<float>(jitter_ >> 8) / 16777216.0f;
    // Lower for heavier: a 1 kg box as made, 100 kg about an octave lower
    const float pitch = std::clamp(std::pow(hit.mass, -0.15f), 0.4f, 2.5f) * jitter;
    // (Played or not, the pair's had its turn: a hit too quiet to take a
    // voice now doesn't try again every frame)
    lastKnock_[hit.pair] = clock_;
    knocks.push_back(
        {.volume = 0.15f + 0.85f * loudness * loudness, .pan = panFor(hit.point), .pitch = pitch});
  }
  hits.clear();
  // Forget pairs that haven't knocked for a while
  std::erase_if(lastKnock_, [&](const auto &entry) { return clock_ - entry.second > 1.0f; });
}

float ImpactKnocks::panFor(const cyclone::Vector3 &position) const {
  // The demos look from +x at the origin, turned theta degrees around the
  // vertical: on screen, right is -z after that turn
  const float theta = cameraAngle_ * 3.14159265f / 180.0f;
  const float z = -std::sin(theta) * static_cast<float>(position.x) +
                  std::cos(theta) * static_cast<float>(position.z);
  return std::clamp(-z / 10.0f, -0.8f, 0.8f); // never all the way to one side
}
