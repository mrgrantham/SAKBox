#include "FlightSim.hpp"

#include <algorithm>
#include <cmath>

namespace PrecisionFlight {

using Flex::Math::FlexOverride;
using Flex::Math::FlexPoint;
using Flex::Math::Policy;
using Flex::Math::Precision;

namespace {

struct Rect {
  float x0, y0, x1, y1;
  bool overlaps(const Rect &o) const { return x0 < o.x1 && o.x0 < x1 && y0 < o.y1 && o.y0 < y1; }
  bool contains(const Rect &o) const {
    return o.x0 >= x0 && o.x1 <= x1 && o.y0 >= y0 && o.y1 <= y1;
  }
};

} // namespace

void FlightSim::reset() {
  x_ = y_ = vx_ = vy_ = FlexPoint();
  z_ = 0.0;
  observed_ = glm::vec2(0.0f);
  observedZ_ = 0.0f;
  score_ = gates_ = goldGates_ = phasedWalls_ = 0;
  shield_ = 100.0f;
  over_ = false;
  focus_ = phase_ = Effect();
  obstacles_.clear();
  nextObstacleId_ = 0;
  nextSpawnZ_ = -40.0;
  rng_.seed(7);
  Flex::Math::seedStochasticRounding(7); // so stochastic-rounding runs replay too
  updateGroups();
}

void FlightSim::updateGroups() {
  auto bits = [&](int fracBits) {
    return Precision{.fracBits = fracBits, .rounding = settings.rounding};
  };
  flight_.setBase(bits(settings.flightBase));
  hull_.setBase(bits(settings.hullBase));
  render_.setBase(bits(settings.renderBase));
  flight_.setPolicy(settings.lossyPosition ? Policy::Lossy : Policy::View);
  // Re-pushed every step so lab changes apply mid-effect; assigning the new
  // handle releases the previous override
  focusFlight_ = focus_.active() ? flight_.push(bits(settings.focusFlightBits)) : FlexOverride();
  phaseHull_ = phase_.active() ? hull_.push(bits(settings.phaseHullBits)) : FlexOverride();
  phaseRender_ = phase_.active() ? render_.push(bits(settings.phaseRenderBits)) : FlexOverride();
}

float FlightSim::random(float lo, float hi) {
  return std::uniform_real_distribution<float>(lo, hi)(rng_);
}

float FlightSim::randomOnGrid(float lo, float hi, float grid) {
  return std::round(random(lo, hi) / grid) * grid;
}

void FlightSim::spawnSegment() {
  const float z0 = static_cast<float>(nextSpawnZ_);
  const float roll = random(0.0f, 1.0f);
  auto add = [&](Obstacle o) {
    o.id = nextObstacleId_++;
    obstacles_.push_back(o);
  };
  auto gate = [&](float gz) {
    Obstacle o{.kind = Kind::Gate, .z = gz};
    o.w = 3.4f;
    o.h = 2.6f;
    o.cx = randomOnGrid(-kLaneHalfWidth + 2.0f, kLaneHalfWidth - 2.0f, 0.5f);
    o.cy = randomOnGrid(kLaneMinY + 1.0f, kLaneMaxY - 1.0f, 0.5f);
    add(o);
  };
  if (roll < 0.35f) {
    gate(z0);
    nextSpawnZ_ -= 32.0;
  } else if (roll < 0.55f) {
    gate(z0);
    gate(z0 - 16.0f);
    nextSpawnZ_ -= 44.0;
  } else if (roll < 0.78f) {
    Obstacle pickup{.kind = Kind::FocusPickup, .z = z0};
    pickup.cx = randomOnGrid(-4.0f, 4.0f, 0.5f);
    pickup.cy = randomOnGrid(-2.0f, 2.5f, 0.5f);
    add(pickup);
    // Opening centred a quarter unit off the default 0.5 flight grid, with
    // 0.2 of slack: unreachable until FOCUS refines the grid.
    Obstacle gold{.kind = Kind::GoldGate, .z = z0 - 45.0f};
    gold.cx = randomOnGrid(-4.0f, 4.0f, 0.5f) + 0.25f;
    gold.cy = randomOnGrid(-1.5f, 2.0f, 0.5f);
    gold.w = 2.0f * (kShipHalfX + 0.2f);
    gold.h = 2.0f * (kShipHalfY + 0.9f);
    add(gold);
    nextSpawnZ_ -= 70.0;
  } else {
    Obstacle pickup{.kind = Kind::PhasePickup, .z = z0};
    pickup.cx = randomOnGrid(-4.0f, 4.0f, 0.5f);
    pickup.cy = randomOnGrid(-2.0f, 2.5f, 0.5f);
    add(pickup);
    // Walls sit on odd z so 2-unit hull sampling always steps over them
    float wz = z0 - 40.0f;
    float k = std::round((-wz - 1.0f) / 2.0f);
    add(Obstacle{.kind = Kind::PhaseWall, .z = -(2.0f * k + 1.0f)});
    nextSpawnZ_ -= 70.0;
  }
}

void FlightSim::integrateAxis(FlexPoint &p, FlexPoint &v, float input, float lo, float hi) {
  static const FlexPoint dt = kStepSeconds;
  static const FlexPoint drag = 4.0;
  static const FlexPoint maxSpeed = 14.0;
  v += FlexPoint(70.0 * input) * dt;
  v -= v * drag * dt;
  if (settings.quantizeVelocity) {
    v = flight_.view(v);
  }
  v = std::clamp(v, -maxSpeed, maxSpeed);
  p += v * dt;
  p = flight_.commit(p); // snaps only under the Lossy policy
  if (p < FlexPoint(lo)) {
    p = lo;
    v = FlexPoint();
  } else if (p > FlexPoint(hi)) {
    p = hi;
    v = FlexPoint();
  }
}

void FlightSim::crash(Obstacle &o, float damage, bool wall, std::vector<FlightEvent> &events) {
  shield_ = std::max(0.0f, shield_ - damage);
  events.push_back(Crashed{.obstacleId = o.id, .wall = wall, .damage = damage, .distance = -z_});
  if (shield_ <= 0.0f) {
    over_ = true;
    events.push_back(GameOver{.distance = -z_});
  }
}

void FlightSim::checkCollisions(std::vector<FlightEvent> &events) {
  const Rect ship{observed_.x - kShipHalfX, observed_.y - kShipHalfY, observed_.x + kShipHalfX,
                  observed_.y + kShipHalfY};
  const float window = kWallHalfZ + kShipHalfZ;
  for (Obstacle &o : obstacles_) {
    if (o.resolved) {
      continue;
    }
    if (o.kind == Kind::FocusPickup || o.kind == Kind::PhasePickup) {
      float lateral = glm::length(observed_ - glm::vec2(o.cx, o.cy));
      if (std::abs(observedZ_ - o.z) < 0.9f && lateral < 1.1f) {
        o.resolved = true;
        if (o.kind == Kind::FocusPickup) {
          focus_ = {settings.focusSeconds, settings.focusSeconds};
        } else {
          phase_ = {settings.phaseSeconds, settings.phaseSeconds};
        }
        events.push_back(PowerupCollected{.obstacleId = o.id, .kind = o.kind, .distance = -z_});
      } else if (z_ < o.z - 3.0) {
        o.resolved = true;
      }
      continue;
    }

    if (std::abs(observedZ_ - o.z) <= window) {
      o.touched = true;
      o.resolved = true;
      if (o.kind == Kind::PhaseWall) {
        crash(o, 34.0f, true, events);
        continue;
      }
      Rect hole{o.cx - o.w / 2, o.cy - o.h / 2, o.cx + o.w / 2, o.cy + o.h / 2};
      Rect frame{hole.x0 - kGateBar, hole.y0 - kGateBar, hole.x1 + kGateBar, hole.y1 + kGateBar};
      if (hole.contains(ship)) {
        bool gold = o.kind == Kind::GoldGate;
        const int points = gold ? 300 : 100;
        score_ += points;
        gold ? ++goldGates_ : ++gates_;
        events.push_back(
            GatePassed{.obstacleId = o.id, .gold = gold, .points = points, .distance = -z_});
      } else if (frame.overlaps(ship)) {
        crash(o, 20.0f, false, events);
      }
    } else if (!o.touched && z_ < o.z - window - 0.5) {
      // Passed without the hull ever sampling a position inside it
      o.resolved = true;
      if (o.kind == Kind::PhaseWall) {
        score_ += 150;
        ++phasedWalls_;
        events.push_back(WallPhased{.obstacleId = o.id, .points = 150, .distance = -z_});
      }
    }
  }
}

// Aim for the nearest gate opening or pickup ahead. Walls are left to the
// PHASE pickup that precedes them.
void FlightSim::autopilotInput(float &inputX, float &inputY) const {
  const Obstacle *next = nullptr;
  for (const Obstacle &o : obstacles_) {
    if (!o.resolved && o.kind != Kind::PhaseWall && o.z < z_ && (!next || o.z > next->z)) {
      next = &o;
    }
  }
  if (!next) {
    return;
  }
  auto steer = [](float error, float velocity) {
    return std::clamp(error * 2.0f - velocity * 0.25f, -1.0f, 1.0f);
  };
  inputX = steer(next->cx - static_cast<float>(x_.value()), static_cast<float>(vx_.value()));
  inputY = steer(next->cy - static_cast<float>(y_.value()), static_cast<float>(vy_.value()));
}

void FlightSim::advance(double seconds, float inputX, float inputY,
                        std::vector<FlightEvent> &events) {
  accumulator_ += seconds;
  while (accumulator_ >= kStepSeconds) {
    step(inputX, inputY, events);
    accumulator_ -= kStepSeconds;
  }
}

void FlightSim::step(float inputX, float inputY, std::vector<FlightEvent> &events) {
  if (over_ || paused) {
    return;
  }
  const float dt = static_cast<float>(kStepSeconds);
  focus_.remaining = std::max(0.0f, focus_.remaining - dt);
  phase_.remaining = std::max(0.0f, phase_.remaining - dt);
  updateGroups();

  if (settings.autopilot) {
    autopilotInput(inputX, inputY);
  }
  integrateAxis(x_, vx_, inputX, -kLaneHalfWidth, kLaneHalfWidth);
  integrateAxis(y_, vy_, inputY, kLaneMinY, kLaneMaxY);
  z_ -= settings.speed * kStepSeconds;

  observed_ = glm::vec2(flight_.view(x_).value(), flight_.view(y_).value());
  observedZ_ = static_cast<float>(hull_.view(FlexPoint(z_)).value());

  checkCollisions(events);

  while (nextSpawnZ_ > z_ - 260.0) {
    spawnSegment();
  }
  obstacles_.erase(std::remove_if(obstacles_.begin(), obstacles_.end(),
                                  [&](const Obstacle &o) { return o.z > z_ + 20.0; }),
                   obstacles_.end());
}

WorldSnapshot FlightSim::snapshot() const {
  WorldSnapshot s;
  s.z = z_;
  s.observed = observed_;
  s.observedZ = observedZ_;
  s.vx = static_cast<float>(vx_.value());
  s.vy = static_cast<float>(vy_.value());
  s.obstacles = obstacles_;
  s.score = score_;
  s.shield = shield_;
  s.gates = gates_;
  s.goldGates = goldGates_;
  s.phasedWalls = phasedWalls_;
  s.focus = focus_;
  s.phase = phase_;
  s.groups = {GroupBits{flight_.name(), flight_.precision().fracBits},
              GroupBits{hull_.name(), hull_.precision().fracBits},
              GroupBits{render_.name(), render_.precision().fracBits}};
  s.renderBits = render_.precision().fracBits;
  s.paused = paused;
  s.over = over_;
  s.autopilot = settings.autopilot;
  return s;
}

} // namespace PrecisionFlight
