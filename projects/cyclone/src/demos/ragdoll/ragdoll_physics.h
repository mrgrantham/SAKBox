// The ragdoll's physics: a World of twelve box bones held together by
// eleven joints, colliding with the ground (as boxes) and with each other
// (as spheres). Shared by the ragdoll demo, which adds rendering, by
// tests/ragdoll_test.cpp, by the Cyclone padds example and by the DS app.
#pragma once

#include <algorithm>
#include <cyclone/cyclone.h>

namespace ragdoll {

constexpr unsigned kBones = 12;
constexpr unsigned kJoints = 11;

// What each of a bone's shapes collides with (see cyclone::CollisionFilter):
// its box with the ground (category 1, a plane's default), its sphere with
// the other bones' spheres
constexpr cyclone::CollisionFilter kBoneBox{.category = 2, .mask = 1};
constexpr cyclone::CollisionFilter kBoneSphere{.category = 4, .mask = 4};

class Bone : public cyclone::CollisionBox {
public:
  /**
   * We use a sphere to collide bone on bone to allow some limited
   * interpenetration. (SAKBox: the largest that fits in the box, on the
   * same body.)
   */
  cyclone::CollisionSphere sphere;

  Bone() {
    body = &rigidBody;
    sphere.body = &rigidBody;
    filter = kBoneBox;
    sphere.filter = kBoneSphere;
  }
  Bone(const Bone &) = delete;
  Bone &operator=(const Bone &) = delete;

  /** Sets the bone to a specific location. */
  void setState(const cyclone::Vector3 &position, const cyclone::Vector3 &extents) {
    body->setPosition(position);
    body->setOrientation(cyclone::Quaternion());
    body->setVelocity(cyclone::Vector3());
    body->setRotation(cyclone::Vector3());
    halfSize = extents;
    sphere.radius = std::min({halfSize.x, halfSize.y, halfSize.z});

    cyclone::real mass = halfSize.x * halfSize.y * halfSize.z * 8.0f;
    body->setMass(mass);

    cyclone::Matrix3 tensor;
    tensor.setBlockInertiaTensor(halfSize, mass);
    body->setInertiaTensor(tensor);

    body->setLinearDamping(0.95f);
    body->setAngularDamping(0.8f);
    body->clearAccumulators();
    body->setAcceleration(cyclone::Vector3::GRAVITY);

    body->setCanSleep(false);
    body->setAwake();

    body->calculateDerivedData();
    calculateInternals();
    sphere.calculateInternals();
  }

private:
  cyclone::RigidBody rigidBody;
};

// The most contacts a step can have, and the iterations the book's resolver
// takes (as many as the demos always gave it)
constexpr unsigned kMaxContacts = 256;

class Ragdoll {
public:
  Bone bones[kBones];
  cyclone::Joint joints[kJoints];

  /**
   * The world the bones and joints are in, on the ground (y = 0). Bones
   * bounce off the ground a little (restitution 0.6).
   */
  cyclone::World world{{.maxContacts = kMaxContacts,
                        .friction = (cyclone::real)0.9,
                        .restitution = (cyclone::real)0.6,
                        .iterations = kMaxContacts * 8}};

  Ragdoll() {
    // Right Knee
    joints[0].set(bones[0].body, cyclone::Vector3(0, 1.07f, 0), bones[1].body,
                  cyclone::Vector3(0, -1.07f, 0), 0.15f);

    // Left Knee
    joints[1].set(bones[2].body, cyclone::Vector3(0, 1.07f, 0), bones[3].body,
                  cyclone::Vector3(0, -1.07f, 0), 0.15f);

    // Right elbow
    joints[2].set(bones[9].body, cyclone::Vector3(0, 0.96f, 0), bones[8].body,
                  cyclone::Vector3(0, -0.96f, 0), 0.15f);

    // Left elbow
    joints[3].set(bones[11].body, cyclone::Vector3(0, 0.96f, 0), bones[10].body,
                  cyclone::Vector3(0, -0.96f, 0), 0.15f);

    // Stomach to Waist
    joints[4].set(bones[4].body, cyclone::Vector3(0.054f, 0.50f, 0), bones[5].body,
                  cyclone::Vector3(-0.043f, -0.45f, 0), 0.15f);

    joints[5].set(bones[5].body, cyclone::Vector3(-0.043f, 0.411f, 0), bones[6].body,
                  cyclone::Vector3(0, -0.411f, 0), 0.15f);

    joints[6].set(bones[6].body, cyclone::Vector3(0, 0.521f, 0), bones[7].body,
                  cyclone::Vector3(0, -0.752f, 0), 0.15f);

    // Right hip
    joints[7].set(bones[1].body, cyclone::Vector3(0, 1.066f, 0), bones[4].body,
                  cyclone::Vector3(0, -0.458f, -0.5f), 0.15f);

    // Left Hip
    joints[8].set(bones[3].body, cyclone::Vector3(0, 1.066f, 0), bones[4].body,
                  cyclone::Vector3(0, -0.458f, 0.5f), 0.105f);

    // Right shoulder
    joints[9].set(bones[6].body, cyclone::Vector3(0, 0.367f, -0.8f), bones[8].body,
                  cyclone::Vector3(0, 0.888f, 0.32f), 0.15f);

    // Left shoulder
    joints[10].set(bones[6].body, cyclone::Vector3(0, 0.367f, 0.8f), bones[10].body,
                   cyclone::Vector3(0, 0.888f, -0.32f), 0.15f);

    cyclone::CollisionPlane ground;
    ground.direction = cyclone::Vector3(0, 1, 0);
    ground.offset = 0;
    world.addPlane(ground);
    for (Bone &bone : bones) {
      world.addBody(bone.body);
      world.addShape(&bone);
      world.addShape(&bone.sphere);
    }
    // Joined bones collide too, as they always have: the spheres of a joint's
    // two bones keep them from folding through each other
    for (cyclone::Joint &joint : joints) {
      joint.collideConnected = true;
      world.addJoint(&joint);
    }
  }
  Ragdoll(const Ragdoll &) = delete;
  Ragdoll &operator=(const Ragdoll &) = delete;

  // Stands the ragdoll up and gives it a random push so it falls over.
  // SAKBox: upstream pushed with forces of 500-1000 N, whose effect scales
  // with the length of the first frame (0.05 s frames launched it at
  // 65 m/s). The push is now a change in velocity, the same at any frame
  // rate: a sideways shove plus a twist of the chest.
  void reset(cyclone::Random &random) {
    for (cyclone::Joint &joint : joints)
      joint.impulse = cyclone::Vector3(); // a fresh start for warm starting
    world.getResolver().forgetWarmStart();
    bones[0].setState(cyclone::Vector3(0, 0.993, -0.5), cyclone::Vector3(0.301, 1.0, 0.234));
    bones[1].setState(cyclone::Vector3(0, 3.159, -0.56), cyclone::Vector3(0.301, 1.0, 0.234));
    bones[2].setState(cyclone::Vector3(0, 0.993, 0.5), cyclone::Vector3(0.301, 1.0, 0.234));
    bones[3].setState(cyclone::Vector3(0, 3.15, 0.56), cyclone::Vector3(0.301, 1.0, 0.234));
    bones[4].setState(cyclone::Vector3(-0.054, 4.683, 0.013),
                      cyclone::Vector3(0.415, 0.392, 0.690));
    bones[5].setState(cyclone::Vector3(0.043, 5.603, 0.013), cyclone::Vector3(0.301, 0.367, 0.693));
    bones[6].setState(cyclone::Vector3(0, 6.485, 0.013), cyclone::Vector3(0.435, 0.367, 0.786));
    bones[7].setState(cyclone::Vector3(0, 7.759, 0.013), cyclone::Vector3(0.45, 0.598, 0.421));
    bones[8].setState(cyclone::Vector3(0, 5.946, -1.066), cyclone::Vector3(0.267, 0.888, 0.207));
    bones[9].setState(cyclone::Vector3(0, 4.024, -1.066), cyclone::Vector3(0.267, 0.888, 0.207));
    bones[10].setState(cyclone::Vector3(0, 5.946, 1.066), cyclone::Vector3(0.267, 0.888, 0.207));
    bones[11].setState(cyclone::Vector3(0, 4.024, 1.066), cyclone::Vector3(0.267, 0.888, 0.207));

    const cyclone::real shove = -random.randomReal(1.5, 3.0); // m/s along x
    for (Bone &bone : bones) {
      bone.body->addVelocity(cyclone::Vector3(shove, 0, 0));
    }
    bones[6].body->addVelocity(cyclone::Vector3(0, 0, random.randomBinomial(1.5)));
    bones[6].body->addRotation(
        cyclone::Vector3(random.randomBinomial(2.0), random.randomBinomial(2.0), 0));
  }

  // One step of `seconds`, in the world's sub-steps
  void step(cyclone::real_duration seconds) {
    world.startFrame();
    world.runPhysics(seconds);
  }

  // How far the most stretched joint's two points are beyond its slack (m),
  // where the bones are now (the resolver moves them without updating their
  // transforms; the next integrate does)
  double worstStretch() const {
    const auto pointNow = [](const cyclone::RigidBody *body, const cyclone::Vector3 &local) {
      cyclone::Quaternion orientation = body->getOrientation();
      orientation.normalise();
      cyclone::Matrix4 transform;
      transform.setOrientationAndPos(orientation, body->getPosition());
      return transform.transform(local);
    };
    double worst = 0;
    for (const cyclone::Joint &joint : joints) {
      const double length =
          (pointNow(joint.body[1], joint.position[1]) - pointNow(joint.body[0], joint.position[0]))
              .magnitude();
      worst = std::max<double>(worst, length - joint.error);
    }
    return worst;
  }
};

} // namespace ragdoll
