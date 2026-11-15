/*
 * Implementation file for random number generation.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cassert>
#include <ctime>
#include <cyclone/random.h>

using namespace cyclone;

// SAKBox: the numbers come from SeededRandom's generator (xoshiro128**),
// the same on every platform, in place of upstream's RandRotB (a 17-word
// buffer filled by a linear congruential generator). Sequences from a given
// seed are new; nothing in SAKBox kept upstream's.

Random::Random() : generator_(0) { seed(0); }

Random::Random(unsigned seed) : generator_(0) { Random::seed(seed); }

Random::Random(const SeededRandom::Generator &generator) : generator_(generator) {}

void Random::seed(unsigned s) {
  if (s == 0) {
    s = (unsigned)clock();
  }
  generator_ = SeededRandom::Generator(s);
}

unsigned Random::randomBits() { return generator_.next(); }

// SAKBox: [0, 1) with all the bits the type holds: 24 for a float, 53 for a
// double (which the fixed-point types round from).
#ifdef SINGLE_PRECISION
real Random::randomReal() { return generator_.unitFloat(); }
#else
real Random::randomReal() { return generator_.unitDouble(); }
#endif

real Random::randomReal(real min, real max) { return randomReal() * (max - min) + min; }

real Random::randomReal(real scale) { return randomReal() * scale; }

unsigned Random::randomInt(unsigned max) {
  // SAKBox: there's no integer below 0 to return (it divided by zero); and
  // every result is equally likely (it was randomBits() % max)
  assert(max > 0);
  return generator_.below(max);
}

real Random::randomBinomial(real scale) { return (randomReal() - randomReal()) * scale; }

// SAKBox: uniformly distributed over all orientations (Shoemake's method).
// Upstream normalised four components drawn from [0, 1), so every one was
// positive: only rotations of up to 180 degrees about axes in one octant.
Quaternion Random::randomQuaternion() {
  real u1 = randomReal(), u2 = randomReal() * 2 * R_PI, u3 = randomReal() * 2 * R_PI;
  real a = real_sqrt(1 - u1), b = real_sqrt(u1);
  return Quaternion(a * real_sin(u2), a * real_cos(u2), b * real_sin(u3), b * real_cos(u3));
}

Vector3 Random::randomVector(real scale) {
  return Vector3(randomBinomial(scale), randomBinomial(scale), randomBinomial(scale));
}

Vector3 Random::randomXZVector(real scale) {
  return Vector3(randomBinomial(scale), 0, randomBinomial(scale));
}

Vector3 Random::randomVector(const Vector3 &scale) {
  return Vector3(randomBinomial(scale.x), randomBinomial(scale.y), randomBinomial(scale.z));
}

Vector3 Random::randomVector(const Vector3 &min, const Vector3 &max) {
  return Vector3(randomReal(min.x, max.x), randomReal(min.y, max.y), randomReal(min.z, max.z));
}
