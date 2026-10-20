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

#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <cyclone/random.h>

using namespace cyclone;

Random::Random() { seed(0); }

Random::Random(unsigned seed) { Random::seed(seed); }

void Random::seed(unsigned s) {
  if (s == 0) {
    s = (unsigned)clock();
  }

  // Fill the buffer with some basic random numbers
  for (unsigned i = 0; i < 17; i++) {
    // Simple linear congruential generator
    // SAKBox: an unsigned constant; as a (signed) long the product overflowed.
    // Same results, since only the low 32 bits are kept.
    s = s * 2891336453u + 1;
    buffer[i] = s;
  }

  // Initialize pointers into the buffer
  p1 = 0;
  p2 = 10;
}

unsigned Random::rotl(unsigned n, unsigned r) { return (n << r) | (n >> (32 - r)); }

unsigned Random::rotr(unsigned n, unsigned r) { return (n >> r) | (n << (32 - r)); }

unsigned Random::randomBits() {
  unsigned result;

  // Rotate the buffer and store it back to itself
  result = buffer[p1] = rotl(buffer[p2], 13) + rotl(buffer[p1], 9);

  // Rotate pointers
  if (--p1 < 0)
    p1 = 16;
  if (--p2 < 0)
    p2 = 16;

  // Return result
  return result;
}

// SAKBox: the bits are reinterpreted with std::bit_cast, not by writing one
// member of a union and reading another (undefined behaviour in C++). The
// results are the same.
#ifdef SINGLE_PRECISION
real Random::randomReal() {
  // Get the random number
  unsigned bits = randomBits();

  // Fix the ieee sign and exponent bits (so that the size of the result
  // is 1-2) and use the bits to create the fraction part of the float.
  return std::bit_cast<float>((bits >> 9) | 0x3f800000u) - 1.0f;
}
#else
real Random::randomReal() {
  // Get the random number
  unsigned bits = randomBits();

  // Fix the ieee sign and exponent bits (so that the size of the result
  // is 1-2) and use the bits, shifted to the top of the 52-bit fraction,
  // to create the fraction part of the double.
  return std::bit_cast<double>(0x3FF0000000000000ull | (std::uint64_t{bits} << 20)) - 1.0;
}
#endif

real Random::randomReal(real min, real max) { return randomReal() * (max - min) + min; }

real Random::randomReal(real scale) { return randomReal() * scale; }

unsigned Random::randomInt(unsigned max) {
  // SAKBox: there's no integer below 0 to return (it divided by zero)
  assert(max > 0);
  return randomBits() % max;
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
