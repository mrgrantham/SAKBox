// World: the shapes, planes, filters and joints it finds contacts between,
// and stepping in sub-steps with the contacts found once a step.
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>
#include <cyclone/world.h>

#include <algorithm>
#include <deque>
#include <vector>

using namespace cyclone;

namespace {

constexpr ContactResolver::Method together = ContactResolver::Method::sequentialImpulses;
constexpr ContactResolver::Method booksResolver = ContactResolver::Method::oneContactAtATime;

// A world of bodies, each with one shape, falling under gravity onto the
// ground (y = 0)
struct Scene {
  World world;
  std::deque<RigidBody> bodies;
  std::deque<CollisionSphere> spheres;
  std::deque<CollisionBox> boxes;
  std::deque<CollisionCapsule> capsules;

  explicit Scene(World::Settings settings = {}) : world(settings) {
    CollisionPlane ground;
    ground.direction = Vector3(0, 1, 0);
    ground.offset = 0;
    world.addPlane(ground);
  }

  RigidBody &body(const Vector3 &position) {
    RigidBody &body = bodies.emplace_back();
    body.setMass(1);
    Matrix3 inertia;
    inertia.setDiagonal(0.4, 0.4, 0.4);
    body.setInertiaTensor(inertia);
    body.setDamping(0.99, 0.8);
    body.setPosition(position);
    body.setAcceleration(Vector3::GRAVITY);
    body.setCanSleep(false);
    body.setAwake();
    body.calculateDerivedData();
    world.addBody(&body);
    return body;
  }
  CollisionSphere &sphere(const Vector3 &position, real radius) {
    CollisionSphere &shape = spheres.emplace_back();
    shape.body = &body(position);
    shape.radius = radius;
    world.addShape(&shape);
    return shape;
  }
  CollisionBox &box(const Vector3 &position, const Vector3 &halfSize) {
    CollisionBox &shape = boxes.emplace_back();
    shape.body = &body(position);
    shape.halfSize = halfSize;
    world.addShape(&shape);
    return shape;
  }
  CollisionCapsule &capsule(const Vector3 &position, real radius, real halfLength) {
    CollisionCapsule &shape = capsules.emplace_back();
    shape.body = &body(position);
    shape.radius = radius;
    shape.halfLength = halfLength;
    world.addShape(&shape);
    return shape;
  }

  // Runs for `seconds` at 60 steps a second
  void run(double seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 60); ++i) {
      world.startFrame();
      world.runPhysics(1.0 / 60);
    }
  }

  // The contacts between the shapes of `one` and `two` (null for the ground)
  unsigned contactsBetween(const RigidBody *one, const RigidBody *two) {
    const unsigned found = world.generateContacts();
    const Contact *contacts = world.getContacts();
    return static_cast<unsigned>(std::count_if(contacts, contacts + found, [&](const Contact &c) {
      return (c.body[0] == one && c.body[1] == two) || (c.body[0] == two && c.body[1] == one);
    }));
  }
};

// Counts how often World asks it for contacts (it has none)
class CountingGenerator : public ContactGenerator {
public:
  mutable int calls = 0;
  unsigned addContact(Contact *, unsigned) const override {
    ++calls;
    return 0;
  }
};

} // namespace

// Every kind of shape touches every other kind, and the ground, each by its
// own test (in the air, 2 m up, apart from the ground)
TEST(WorldTest, FindsContactsBetweenEveryKindOfShape) {
  Scene scene;
  const real up = 2;
  CollisionSphere &sphere = scene.sphere(Vector3(0, up, 0), 0.5);
  CollisionBox &box = scene.box(Vector3(0.9, up, 0), Vector3(0.5, 0.5, 0.5));
  CollisionCapsule &capsule = scene.capsule(Vector3(0, up + 0.9, 0), 0.5, 0.5);
  // Each pair overlaps: sphere and box by 0.1, sphere and capsule (its lower
  // end at 2.4) by 0.6... and the capsule's end rests on the box's top
  EXPECT_GT(scene.contactsBetween(sphere.body, box.body), 0u);
  EXPECT_GT(scene.contactsBetween(sphere.body, capsule.body), 0u);
  EXPECT_GT(scene.contactsBetween(box.body, capsule.body), 0u);
  EXPECT_EQ(scene.contactsBetween(sphere.body, nullptr), 0u); // up in the air

  Scene grounded;
  const RigidBody *onGround[] = {grounded.sphere(Vector3(0, 0.45, 0), 0.5).body,
                                 grounded.box(Vector3(3, 0.45, 0), Vector3(0.5, 0.5, 0.5)).body,
                                 grounded.capsule(Vector3(6, 0.95, 0), 0.5, 0.5).body,
                                 grounded.sphere(Vector3(9, 0.6, 0), 0.5).body};
  for (int i = 0; i < 3; ++i)
    EXPECT_GT(grounded.contactsBetween(onGround[i], nullptr), 0u) << "shape " << i;
  EXPECT_EQ(grounded.contactsBetween(onGround[3], nullptr), 0u); // 10 cm clear
}

// Filters keep shapes apart: each one's category must be in the other's
// mask, the ground's too
TEST(WorldTest, FiltersKeepShapesApart) {
  Scene scene;
  CollisionSphere &one = scene.sphere(Vector3(0, 2, 0), 0.5);
  CollisionSphere &two = scene.sphere(Vector3(0.8, 2, 0), 0.5);
  EXPECT_GT(scene.contactsBetween(one.body, two.body), 0u);
  two.filter.category = 2;
  one.filter.mask = 1; // takes no notice of category 2
  EXPECT_EQ(scene.contactsBetween(one.body, two.body), 0u);
  one.filter.mask = 3;
  EXPECT_GT(scene.contactsBetween(one.body, two.body), 0u);

  CollisionSphere &low = scene.sphere(Vector3(5, 0.4, 0), 0.5);
  EXPECT_GT(scene.contactsBetween(low.body, nullptr), 0u);
  low.filter.mask = 2; // the ground is category 1
  EXPECT_EQ(scene.contactsBetween(low.body, nullptr), 0u);
}

// A body's shapes don't collide with each other, and joined bodies'
// shapes don't, unless the joint says they do (collideConnected)
TEST(WorldTest, ShapesOfOneBodyOrJoinedBodiesDontCollide) {
  Scene scene;
  CollisionSphere &one = scene.sphere(Vector3(0, 2, 0), 0.5);
  CollisionBox &alsoOne = scene.boxes.emplace_back();
  alsoOne.body = one.body;
  alsoOne.halfSize = Vector3(0.3, 0.3, 0.3);
  scene.world.addShape(&alsoOne);
  EXPECT_EQ(scene.contactsBetween(one.body, one.body), 0u);

  CollisionSphere &two = scene.sphere(Vector3(0.8, 2, 0), 0.5);
  Joint joint;
  joint.set(one.body, Vector3(0.4, 0, 0), two.body, Vector3(-0.4, 0, 0), 0);
  scene.world.addJoint(&joint);
  EXPECT_EQ(scene.contactsBetween(one.body, two.body), 0u);
  joint.collideConnected = true;
  EXPECT_GT(scene.contactsBetween(one.body, two.body), 0u);
}

// Shapes come to rest on the ground, and on each other
TEST(WorldTest, ShapesComeToRest) {
  for (unsigned subSteps : {1u, 4u}) {
    SCOPED_TRACE(testing::Message() << subSteps << " sub-steps");
    Scene scene({.subSteps = subSteps});
    scene.world.getResolver().setMethod(together);
    const RigidBody &sphere = *scene.sphere(Vector3(0, 2, 0), 0.5).body;
    const RigidBody &box = *scene.box(Vector3(3, 2, 0), Vector3(0.5, 0.5, 0.5)).body;
    const RigidBody &onBox = *scene.box(Vector3(3, 3.5, 0), Vector3(0.5, 0.5, 0.5)).body;
    scene.run(4);
    EXPECT_NEAR(sphere.getPosition().y, 0.5, 0.01);
    EXPECT_NEAR(box.getPosition().y, 0.5, 0.01);
    EXPECT_NEAR(onBox.getPosition().y, 1.5, 0.02);
    for (const RigidBody *body : {&sphere, &box, &onBox})
      EXPECT_LT(body->getVelocity().magnitude(), 0.02);
  }
}

// Resolving contacts together in sub-steps, the contacts are found once a
// step, after the first sub-step's integration; the book's resolver finds
// them in each. beforeSubStep is called before each sub-step, with its
// duration.
TEST(WorldTest, SubStepsFindContactsOnce) {
  for (ContactResolver::Method method : {together, booksResolver}) {
    Scene scene({.subSteps = 4});
    scene.world.getResolver().setMethod(method);
    scene.sphere(Vector3(0, 0.45, 0), 0.5);
    CountingGenerator counting;
    scene.world.getContactGenerators().push_back(&counting);
    std::vector<real_duration> subSteps;
    scene.world.beforeSubStep = [&](real_duration seconds) { subSteps.push_back(seconds); };
    scene.world.startFrame();
    scene.world.runPhysics(1.0 / 60);
    EXPECT_EQ(counting.calls, method == together ? 1 : 4);
    ASSERT_EQ(subSteps.size(), 4u);
    EXPECT_NEAR(subSteps[0], 1.0 / 240, 1e-12);
    EXPECT_GT(scene.world.getStepContacts().size(), 0u);
    EXPECT_GE(scene.world.profile().resolve, 0);
  }
}

// A box falling fast lands in sub-steps without sinking into the ground:
// against a plane the contacts are found as far ahead as the box can fall in
// the step (see World::Settings::margin), and followed exactly
TEST(WorldTest, FastFallingBoxLandsWithoutSinking) {
  Scene scene({.subSteps = 4});
  scene.world.getResolver().setMethod(together);
  RigidBody &box = *scene.box(Vector3(0, 1.5, 0), Vector3(0.5, 0.5, 0.5)).body;
  box.setVelocity(0, -20, 0); // a third of a metre a step
  double lowest = 10;
  for (int i = 0; i < 60; ++i) {
    scene.run(1.0 / 60);
    lowest = std::min<double>(lowest, box.getPosition().y);
  }
  EXPECT_GT(lowest, 0.5 - 0.02);
  EXPECT_NEAR(box.getPosition().y, 0.5, 0.01);
}

// Removing a body removes its shapes, and its force registrations
TEST(WorldTest, RemovingABodyRemovesItsShapes) {
  Scene scene;
  CollisionSphere &kept = scene.sphere(Vector3(0, 0.45, 0), 0.5);
  CollisionSphere &removed = scene.sphere(Vector3(3, 0.45, 0), 0.5);
  scene.world.removeBody(removed.body);
  EXPECT_EQ(scene.world.getShapes().size(), 1u);
  EXPECT_EQ(scene.world.getShapes()[0], &kept);
  EXPECT_EQ(scene.world.getBodies().size(), 1u);
  EXPECT_EQ(scene.contactsBetween(removed.body, nullptr), 0u);
}

// Each step remembers where every body started it, so it can be drawn
// between there and where it is now: a quarter of the way at 0.25, turned a
// quarter of the way too. A body placed by hand is drawn where it is, once
// its new pose is remembered (or before any is).
TEST(WorldTest, BodiesAreDrawnBlendedBetweenSteps) {
  Scene scene;
  RigidBody &body = *scene.sphere(Vector3(0, 5, 0), 0.5).body;
  EXPECT_EQ(body.getBlendedPosition(0).y, 5); // nothing remembered yet: where it is
  body.setRotation(Vector3(0, 6, 0));         // turning about y
  const Vector3 start = body.getPosition();
  scene.world.startFrame();
  scene.world.runPhysics(1.0 / 60);
  const Vector3 now = body.getPosition();
  EXPECT_LT(now.y, start.y);
  EXPECT_NEAR(body.getBlendedPosition(0).y, start.y, 1e-12);
  EXPECT_NEAR(body.getBlendedPosition(1).y, now.y, 1e-12);
  EXPECT_NEAR(body.getBlendedPosition(0.25).y, start.y + (now.y - start.y) * 0.25, 1e-12);

  // The angle turned, from the orientation's real part: cos(angle / 2)
  auto turned = [](const Quaternion &q) { return 2 * std::acos(std::min<double>(q.r, 1)); };
  const double fullTurn = turned(body.getOrientation());
  EXPECT_GT(fullTurn, 0.05);
  EXPECT_NEAR(turned(body.getBlendedOrientation(0.25)), fullTurn * 0.25, 1e-3);
  const Matrix4 blended = body.getBlendedTransform(0.25);
  EXPECT_NEAR(blended.getAxisVector(3).y, body.getBlendedPosition(0.25).y, 1e-12);

  body.setPosition(Vector3(10, 5, 0)); // teleported
  scene.world.rememberPoses();
  EXPECT_EQ(body.getBlendedPosition(0).x, 10);
}
