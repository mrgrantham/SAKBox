#include <CosmicHex/Dice/DiceInTurn.hpp>
#include <CosmicHex/Dice/DiceTray.hpp>

#include <SeededRandom/Generator.hpp>

#include <cmath>
#include <vector>

namespace CosmicHex::Dice {

namespace {
using cyclone::real;
using cyclone::Vector3;

constexpr real kGravity = -9.81 * 2; // (as DiceTray's)
constexpr double kStillSpeed = 0.05; // barely moving: slower than this...
constexpr double kStillFor = 0.4;    // ...for this long
constexpr double kLongestThrow = 8;  // (a throw that won't settle: taken as it is)
constexpr double kSweepTolerance = 0.003;

real between(SeededRandom::Generator &random, double low, double high) {
  return static_cast<real>(low + (high - low) * random.below(1u << 16) / double((1u << 16) - 1));
}

cyclone::CollisionPlane plane(Vector3 direction, real offset) {
  cyclone::CollisionPlane p;
  p.direction = direction;
  p.offset = offset;
  return p;
}

Vector3 normalOf(int face) {
  switch (face) {
  case 1:
    return {1, 0, 0};
  case 6:
    return {-1, 0, 0};
  case 2:
    return {0, 1, 0};
  case 5:
    return {0, -1, 0};
  case 3:
    return {0, 0, 1};
  default:
    return {0, 0, -1};
  }
}

int topFace(const cyclone::RigidBody &body) {
  // The die's own axes in the world: whichever points most nearly up
  const cyclone::Matrix4 &transform = body.getTransform();
  int face = 0;
  real best = -2;
  for (int axis = 0; axis < 3; ++axis) {
    const Vector3 a = transform.getAxisVector(axis);
    if (a.y > best) {
      best = a.y;
      face = axis + 1;
    }
    if (-a.y > best) {
      best = -a.y;
      face = 6 - axis;
    }
  }
  return face;
}
} // namespace

cyclone::Quaternion turnFaceTo(int face, int up) {
  if (face == up)
    return {};
  const Vector3 from = normalOf(face), to = normalOf(up);
  // Opposite: half a turn about an axis across them; else a quarter turn
  // about the axis both are square to
  Vector3 axis = from % to;
  real angle = static_cast<real>(1.5707963);
  if (face + up == 7) {
    axis = std::abs(static_cast<double>(from.x)) > 0.5 ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    angle = static_cast<real>(3.14159265);
  }
  axis.normalise();
  const real s = static_cast<real>(std::sin(static_cast<double>(angle) / 2));
  return {static_cast<real>(std::cos(static_cast<double>(angle) / 2)), axis.x * s, axis.y * s,
          axis.z * s};
}

struct DiceInTurn::Die : cyclone::CollisionBox {
  cyclone::RigidBody rigid;
};

struct DiceInTurn::Throw {
  cyclone::World world{
      cyclone::World::Settings{.maxContacts = 64,
                               .subSteps = 4,
                               .friction = static_cast<real>(DiceTray::kFriction),
                               .restitution = static_cast<real>(DiceTray::kBounce),
                               .iterations = 64 * 8,
                               .sweepTolerance = static_cast<real>(kSweepTolerance)}};
  std::vector<std::unique_ptr<Die>> fixed; // (the dice already settled)
  Die moving;
};

DiceInTurn::DiceInTurn(double stepSeconds) : step_(stepSeconds) {}
DiceInTurn::~DiceInTurn() = default;

std::unique_ptr<DiceInTurn::Throw> DiceInTurn::makeThrow(int die, std::uint64_t seed) const {
  auto t = std::make_unique<Throw>();
  const auto half = static_cast<real>(DiceTray::kDieSize / 2);
  // The glass: a floor, a lid and walls, facing in
  t->world.addPlane(plane({0, 1, 0}, 0));
  t->world.addPlane(plane({0, -1, 0}, static_cast<real>(-DiceTray::kHeight)));
  t->world.addPlane(plane({1, 0, 0}, static_cast<real>(-DiceTray::kWidth / 2)));
  t->world.addPlane(plane({-1, 0, 0}, static_cast<real>(-DiceTray::kWidth / 2)));
  t->world.addPlane(plane({0, 0, 1}, static_cast<real>(-DiceTray::kDepth / 2)));
  t->world.addPlane(plane({0, 0, -1}, static_cast<real>(-DiceTray::kDepth / 2)));
  // The dice already settled, where they lie, too heavy to move
  for (int i = 0; i < kMostDice; ++i) {
    const Slot &slot = slots_[static_cast<std::size_t>(i)];
    if (i == die || !slot.settled)
      continue;
    auto d = std::make_unique<Die>();
    d->body = &d->rigid;
    d->halfSize = Vector3(half, half, half);
    d->rigid.setPosition(slot.restAt);
    d->rigid.setOrientation(slot.restTurn);
    // (a body starts with nothing set: everything given, still and massless)
    d->rigid.setVelocity(Vector3());
    d->rigid.setRotation(Vector3());
    d->rigid.setAcceleration(Vector3());
    d->rigid.setLinearDamping(1);
    d->rigid.setAngularDamping(1);
    d->rigid.clearAccumulators();
    // (heavy rather than massless: Cyclone's contacts went to NaN on a
    // body with no mass at all. A thousand times a die barely moves, and
    // it's drawn where it settled whatever it does)
    constexpr real kHeavy = 1000;
    d->rigid.setMass(kHeavy);
    cyclone::Matrix3 heavy;
    heavy.setBlockInertiaTensor(d->halfSize, kHeavy);
    d->rigid.setInertiaTensor(heavy);
    d->rigid.setCanSleep(true);
    d->rigid.calculateDerivedData();
    d->rigid.setAwake(false);
    t->world.addBody(&d->rigid);
    t->world.addShape(d.get());
    t->fixed.push_back(std::move(d));
  }
  // The die thrown: in from the left, at the seed's height, speed, angle
  // and spin, turned at random (as DiceTray's)
  SeededRandom::Generator random(seed);
  Die &d = t->moving;
  d.body = &d.rigid;
  d.halfSize = Vector3(half, half, half);
  cyclone::RigidBody &body = d.rigid;
  body.setPosition(Vector3(static_cast<real>(-DiceTray::kWidth / 2 + 0.8),
                           between(random, 1.5, 3.2), between(random, -1.6, 1.6)));
  cyclone::Quaternion turn(between(random, -1, 1), between(random, -1, 1), between(random, -1, 1),
                           between(random, -1, 1));
  turn.normalise();
  body.setOrientation(turn);
  body.setVelocity(Vector3(between(random, 8, 14), between(random, -2, 3), between(random, -3, 3)));
  body.setRotation(
      Vector3(between(random, -25, 25), between(random, -25, 25), between(random, -25, 25)));
  body.setMass(1);
  cyclone::Matrix3 tensor;
  tensor.setBlockInertiaTensor(d.halfSize, 1);
  body.setInertiaTensor(tensor);
  body.setLinearDamping(static_cast<real>(0.99));
  body.setAngularDamping(static_cast<real>(0.9));
  body.clearAccumulators();
  body.setAcceleration(0, kGravity, 0);
  body.setCanSleep(true);
  body.setAwake();
  body.calculateDerivedData();
  t->world.addBody(&body);
  t->world.addShape(&d);
  return t;
}

void DiceInTurn::throwDie(int die, std::uint64_t seed, int face) {
  if (die < 0 || die >= kMostDice)
    return;
  if (moving_ >= 0)
    settleNow(moving_);
  Slot &slot = slots_[static_cast<std::size_t>(die)];
  // The rehearsal: the same throw, stepped to rest, unseen
  {
    std::unique_ptr<Throw> rehearsal = makeThrow(die, seed);
    double still = 0;
    for (double t = 0; t < kLongestThrow && still < kStillFor; t += step_) {
      rehearsal->world.startFrame();
      rehearsal->world.runPhysics(static_cast<real>(step_));
      const cyclone::RigidBody &body = rehearsal->moving.rigid;
      const bool moving =
          body.getAwake() && (static_cast<double>(body.getVelocity().magnitude()) > kStillSpeed ||
                              static_cast<double>(body.getRotation().magnitude()) > kStillSpeed);
      still = moving ? 0 : still + step_;
    }
    slot.restAt = rehearsal->moving.rigid.getPosition();
    slot.restTurn = rehearsal->moving.rigid.getOrientation();
    slot.numbersTurned = turnFaceTo(face, topFace(rehearsal->moving.rigid));
  }
  // For real, from the same start
  live_ = makeThrow(die, seed);
  slot.thrown = true;
  slot.settled = false;
  moving_ = die;
  still_ = time_ = 0;
}

void DiceInTurn::step() {
  if (!live_ || moving_ < 0)
    return;
  live_->world.startFrame();
  live_->world.runPhysics(static_cast<real>(step_));
  time_ += step_;
  const cyclone::RigidBody &body = live_->moving.rigid;
  const bool moving =
      body.getAwake() && (static_cast<double>(body.getVelocity().magnitude()) > kStillSpeed ||
                          static_cast<double>(body.getRotation().magnitude()) > kStillSpeed);
  still_ = moving ? 0 : still_ + step_;
  if (still_ >= kStillFor || time_ >= kLongestThrow)
    settleNow(moving_);
}

void DiceInTurn::settleNow(int die) {
  if (die != moving_ || die < 0)
    return;
  // (where the rehearsal left it: the same place the throw comes to)
  slots_[static_cast<std::size_t>(die)].settled = true;
  moving_ = -1;
  live_.reset();
}

bool DiceInTurn::thrown(int die) const {
  return die >= 0 && die < kMostDice && slots_[static_cast<std::size_t>(die)].thrown;
}
bool DiceInTurn::settled(int die) const {
  return die >= 0 && die < kMostDice && slots_[static_cast<std::size_t>(die)].settled;
}

cyclone::Vector3 DiceInTurn::position(int die) const {
  if (die == moving_ && live_)
    return live_->moving.rigid.getPosition();
  return slots_[static_cast<std::size_t>(die)].restAt;
}

cyclone::Quaternion DiceInTurn::orientation(int die) const {
  const Slot &slot = slots_[static_cast<std::size_t>(die)];
  cyclone::Quaternion turn =
      die == moving_ && live_ ? live_->moving.rigid.getOrientation() : slot.restTurn;
  turn *= slot.numbersTurned; // (the body's turn, then the numbers' inside it)
  return turn;
}

} // namespace CosmicHex::Dice
