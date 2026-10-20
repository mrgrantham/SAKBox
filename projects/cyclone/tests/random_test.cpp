// Cyclone's random number generator: repeatable from a seed, and each kind
// of value spread over its whole range.
#include <gtest/gtest.h>

#include <cyclone/random.h>

#include <set>

using namespace cyclone;

namespace {
constexpr int kSamples = 20000;
} // namespace

TEST(RandomTest, TheSameSeedGivesTheSameSequence) {
  Random a(42), b(42), c(43);
  bool differsFromOtherSeed = false;
  for (int i = 0; i < 100; ++i) {
    const unsigned bits = a.randomBits();
    EXPECT_EQ(bits, b.randomBits());
    differsFromOtherSeed = differsFromOtherSeed || bits != c.randomBits();
  }
  EXPECT_TRUE(differsFromOtherSeed);
}

// Reals fall in [0, 1), spread evenly: about a tenth in each tenth.
TEST(RandomTest, RealsAreSpreadOverTheUnitInterval) {
  Random random(1);
  int perTenth[10] = {};
  for (int i = 0; i < kSamples; ++i) {
    const double value = random.randomReal();
    ASSERT_GE(value, 0.0);
    ASSERT_LT(value, 1.0);
    ++perTenth[static_cast<int>(value * 10)];
  }
  for (int count : perTenth)
    EXPECT_NEAR(count, 0.1 * kSamples, 0.01 * kSamples);

  const double scaled = random.randomReal(-2, 3);
  EXPECT_GE(scaled, -2.0);
  EXPECT_LT(scaled, 3.0);
}

TEST(RandomTest, IntegersCoverEveryValueBelowTheLimit) {
  Random random(2);
  std::set<unsigned> seen;
  for (int i = 0; i < 1000; ++i) {
    const unsigned value = random.randomInt(7);
    ASSERT_LT(value, 7u);
    seen.insert(value);
  }
  EXPECT_EQ(seen.size(), 7u);
}

// randomBinomial(s) is the difference of two reals: in (-s, s), centred on 0.
TEST(RandomTest, BinomialIsCentredOnZero) {
  Random random(3);
  double sum = 0;
  for (int i = 0; i < kSamples; ++i) {
    const double value = random.randomBinomial(2);
    ASSERT_GT(value, -2.0);
    ASSERT_LT(value, 2.0);
    sum += value;
  }
  EXPECT_NEAR(sum / kSamples, 0, 0.02);
}

TEST(RandomTest, VectorsFallInTheirBox) {
  Random random(4);
  const Vector3 min(-1, 2, 5), max(1, 3, 9);
  for (int i = 0; i < 1000; ++i) {
    const Vector3 v = random.randomVector(min, max);
    ASSERT_TRUE(v.x >= min.x && v.x < max.x && v.y >= min.y && v.y < max.y && v.z >= min.z &&
                v.z < max.z)
        << v.x << " " << v.y << " " << v.z;
  }
  for (int i = 0; i < 1000; ++i)
    ASSERT_EQ(random.randomXZVector(1).y, 0.0);
}

// Random orientations are unit quaternions spread over every rotation: they
// turn a fixed direction to anywhere on the sphere, evenly, so the turned
// directions average to nothing and half point down. (Upstream's drew all
// four components from [0, 1), so every rotation was about an axis in one
// octant, by up to 180 degrees.)
TEST(RandomTest, OrientationsAreSpreadOverEveryRotation) {
  Random random(5);
  Vector3 sum;
  int pointingDown = 0;
  for (int i = 0; i < kSamples; ++i) {
    const Quaternion q = random.randomQuaternion();
    ASSERT_NEAR(q.r * q.r + q.i * q.i + q.j * q.j + q.k * q.k, 1.0, 1e-9);
    Matrix3 rotation;
    rotation.setOrientation(q);
    const Vector3 turned = rotation.transform(Vector3(0, 0, 1));
    sum += turned;
    pointingDown += turned.z < 0;
  }
  EXPECT_NEAR(sum.x / kSamples, 0, 0.02);
  EXPECT_NEAR(sum.y / kSamples, 0, 0.02);
  EXPECT_NEAR(sum.z / kSamples, 0, 0.02);
  EXPECT_NEAR(pointingDown, 0.5 * kSamples, 0.01 * kSamples);
}
