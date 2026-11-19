#include <CosmicHex/Dice/DiceTray.hpp>

#include <SeededRandom/Generator.hpp>

#include <cmath>

namespace CosmicHex::Dice {

namespace {
using cyclone::real;
using cyclone::Vector3;

constexpr real kGravity = -9.81 * 2; // (a die 1 unit across is about 2 cm: heavier feel)
constexpr double kStillSpeed = 0.05; // barely moving: slower than this...
constexpr double kStillFor = 0.5;    // ...for this long
// The contacts' sweeps stop once none changes by more than this (a die's
// speed by three thousandths of a unit a second): the dice at rest
// spent most of a DS's physics time on sweeps that changed nothing
constexpr double kSweepTolerance = 0.003;

cyclone::CollisionPlane plane(Vector3 direction, real offset) {
  cyclone::CollisionPlane p;
  p.direction = direction;
  p.offset = offset;
  return p;
}

// A number from low to high, from the generator
real between(SeededRandom::Generator &random, double low, double high) {
  return static_cast<real>(low + (high - low) * random.below(1u << 16) / double((1u << 16) - 1));
}
} // namespace

DiceTray::DiceTray()
    : world_(cyclone::World::Settings{.maxContacts = 64,
                                      .subSteps = 4,
                                      .friction = static_cast<real>(kFriction),
                                      .restitution = static_cast<real>(kBounce),
                                      .iterations = 64 * 8,
                                      .sweepTolerance = static_cast<real>(kSweepTolerance)}) {
  // The glass: a floor, a lid, and walls at the screen's edges (each plane
  // facing in)
  world_.addPlane(plane({0, 1, 0}, 0));
  world_.addPlane(plane({0, -1, 0}, static_cast<real>(-kHeight)));
  world_.addPlane(plane({1, 0, 0}, static_cast<real>(-kWidth / 2)));
  world_.addPlane(plane({-1, 0, 0}, static_cast<real>(-kWidth / 2)));
  world_.addPlane(plane({0, 0, 1}, static_cast<real>(-kDepth / 2)));
  world_.addPlane(plane({0, 0, -1}, static_cast<real>(-kDepth / 2)));
  for (Die &die : dice_) {
    die.body = &die.rigid;
    die.halfSize = Vector3(kDieSize / 2, kDieSize / 2, kDieSize / 2);
    world_.addBody(&die.rigid);
    world_.addShape(&die);
  }
}

void DiceTray::throwDice(std::uint64_t seed) {
  SeededRandom::Generator random(seed);
  settled_ = pulling_ = false;
  still_ = time_ = settleTime_ = offset_ = pullSpeed_ = 0;
  for (std::size_t i = 0; i < dice_.size(); ++i) {
    cyclone::RigidBody &body = dice_[i].rigid;
    // In from the left, one nearer the bottom of the screen and one nearer
    // the top, each at its own height, speed, angle and spin, turned at random
    const real z = (i == 0 ? -1 : 1) * between(random, 0.6, 1.8);
    body.setPosition(Vector3(static_cast<real>(-kWidth / 2 + 0.8), between(random, 1.5, 3.2), z));
    cyclone::Quaternion turn(between(random, -1, 1), between(random, -1, 1), between(random, -1, 1),
                             between(random, -1, 1));
    turn.normalise();
    body.setOrientation(turn);
    body.setVelocity(
        Vector3(between(random, 9, 16), between(random, -2, 3), between(random, -3, 3)));
    body.setRotation(
        Vector3(between(random, -25, 25), between(random, -25, 25), between(random, -25, 25)));
    const real mass = 1;
    body.setMass(mass);
    cyclone::Matrix3 tensor;
    tensor.setBlockInertiaTensor(dice_[i].halfSize, mass);
    body.setInertiaTensor(tensor);
    body.setLinearDamping(static_cast<real>(0.99));
    body.setAngularDamping(static_cast<real>(0.9));
    body.clearAccumulators();
    body.setAcceleration(0, kGravity, 0);
    body.setCanSleep(true);
    body.setAwake();
    body.calculateDerivedData();
  }
}

void DiceTray::step(double seconds) {
  time_ += seconds;
  world_.startFrame();
  if (pulling_) {
    // The container speeds off to the left: in its frame, the dice feel
    // pushed the other way (as you do when a car pulls away), and the
    // floor's friction drags them along with it
    pullSpeed_ += DiceTray::kPullAcceleration * seconds;
    offset_ += pullSpeed_ * seconds;
    for (Die &die : dice_) {
      die.rigid.setAwake();
      die.rigid.addForce(
          Vector3(static_cast<real>(DiceTray::kPullAcceleration * die.rigid.getMass()), 0, 0));
    }
  }
  world_.runPhysics(static_cast<real>(seconds));
  if (settled_ || pulling_)
    return;
  bool still = true;
  for (const Die &die : dice_) {
    const auto speed = die.rigid.getVelocity().magnitude();
    const auto spin = die.rigid.getRotation().magnitude();
    if (die.rigid.getAwake() && (speed > kStillSpeed || spin > kStillSpeed))
      still = false;
  }
  still_ = still ? still_ + seconds : 0;
  if (still_ >= kStillFor) {
    settled_ = true;
    settleTime_ = time_;
  }
}

int DiceTray::topFace(const Die &die) const {
  // The die's own axes in the world: whichever points most nearly up is on
  // top. +x 1, -x 6, +y 2, -y 5, +z 3, -z 4 (opposites add to 7)
  const cyclone::Matrix4 &transform = die.rigid.getTransform();
  const std::array<std::pair<Vector3, int>, 3> axes{std::pair{transform.getAxisVector(0), 1},
                                                    std::pair{transform.getAxisVector(1), 2},
                                                    std::pair{transform.getAxisVector(2), 3}};
  int face = 0;
  real best = -2;
  for (const auto &[axis, positive] : axes) {
    if (axis.y > best) {
      best = axis.y;
      face = positive;
    }
    if (-axis.y > best) {
      best = -axis.y;
      face = 7 - positive;
    }
  }
  return face;
}

std::pair<int, int> DiceTray::faces() const { return {topFace(dice_[0]), topFace(dice_[1])}; }

void DiceTray::startPull() { pulling_ = true; }

cyclone::Vector3 DiceTray::position(int die) const {
  return dice_[static_cast<std::size_t>(die)].rigid.getPosition();
}

cyclone::Quaternion DiceTray::orientation(int die) const {
  return dice_[static_cast<std::size_t>(die)].rigid.getOrientation();
}

} // namespace CosmicHex::Dice
