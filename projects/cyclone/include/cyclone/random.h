/*
 * Interface file for the random number generator.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

/**
 * @file
 *
 * This file contains the definitions for a random number generator.
 */
#ifndef CYCLONE_RANDOM_H
#define CYCLONE_RANDOM_H

#include "core.h"

#include <SeededRandom/Generator.hpp>

namespace cyclone {

/**
 * Keeps track of one random stream: i.e. a seed and its output.
 * This is used to get random numbers. Rather than a funcion, this
 * allows there to be several streams of repeatable random numbers
 * at the same time.
 *
 * SAKBox: built on SeededRandom's generator (xoshiro128**), the same
 * numbers on every platform, in place of upstream's RandRotB. A game can
 * hand Cyclone one of its named streams (Random(seed.stream("debris"))).
 */
class Random {
public:
  /**
   * Creates a new random number stream with a seed based on
   * timing data.
   */
  Random();

  /**
   * Creates a new random stream with the given seed.
   */
  Random(unsigned seed);

  /**
   * SAKBox: a stream that carries on from the given generator, such as
   * one of a game's named streams.
   */
  explicit Random(const SeededRandom::Generator &generator);

  /**
   * Sets the seed value for the random stream. (A seed of 0 seeds it
   * from the clock instead.)
   */
  void seed(unsigned seed);

  /**
   * SAKBox: the generator underneath, for saving its state or drawing
   * with SeededRandom's own methods (between, shuffle, ...).
   */
  SeededRandom::Generator &generator() { return generator_; }

  /**
   * Returns the next random bitstring from the stream. This is
   * the fastest method.
   */
  unsigned randomBits();

  /**
   * Returns a random floating point number between 0 and 1.
   */
  real randomReal();

  /**
   * Returns a random floating point number between 0 and scale.
   */
  real randomReal(real scale);

  /**
   * Returns a random floating point number between min and max.
   */
  real randomReal(real min, real max);

  /**
   * Returns a random integer less than the given value, which must be
   * positive. (SAKBox: every result equally likely.)
   */
  unsigned randomInt(unsigned max);

  /**
   * Returns a random binomially distributed number between -scale
   * and +scale.
   */
  real randomBinomial(real scale);

  /**
   * Returns a random vector where each component is binomially
   * distributed in the range (-scale to scale) [mean = 0.0f].
   */
  Vector3 randomVector(real scale);

  /**
   * Returns a random vector where each component is binomially
   * distributed in the range (-scale to scale) [mean = 0.0f],
   * where scale is the corresponding component of the given
   * vector.
   */
  Vector3 randomVector(const Vector3 &scale);

  /**
   * Returns a random vector in the cube defined by the given
   * minimum and maximum vectors. The probability is uniformly
   * distributed in this region.
   */
  Vector3 randomVector(const Vector3 &min, const Vector3 &max);

  /**
   * Returns a random vector where each component is binomially
   * distributed in the range (-scale to scale) [mean = 0.0f],
   * except the y coordinate which is zero.
   */
  Vector3 randomXZVector(real scale);

  /**
   * Returns a random orientation (i.e. normalized) quaternion.
   */
  Quaternion randomQuaternion();

private:
  SeededRandom::Generator generator_;
};

} // namespace cyclone

#endif // CYCLONE_RANDOM_H
