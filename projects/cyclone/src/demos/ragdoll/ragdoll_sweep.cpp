// ragdoll_sweep: the ragdoll's physics with each of the handheld sweep's
// settings (steps a second and sub-steps: physics_sweep.h), over many
// shoves, without a window: how well each holds the ragdoll together and
// lets it come to rest. The DS's sweep measures what each costs there, from
// one shove; this measures the quality, from many, in seconds. Built in the
// DS's Q16.16 as ragdoll_sweep_flex_point16 (or in any real type through
// //projects/cyclone:real), with the resolver's sweeps the DS uses
// (16/4/4 a step, shared among its sub-steps).
//
//   bazel run //projects/cyclone:ragdoll_sweep_flex_point16
//   bazel run //projects/cyclone:ragdoll_sweep_flex_point16 -- --shoves 50 --seconds 15
//
// Prints a Markdown table, a row a setting:
//   worst stretch   the most any joint was beyond its slack, over every shove (cm)
//   mean worst      each shove's worst stretch, averaged (cm)
//   resting speed   the fastest bone, averaged over the last second before
//                   it fell asleep or the run ended (cm/s): jitter
//   asleep          when every bone was asleep, averaged and at worst (s),
//                   and how many shoves never were within the run
//   Mac ms/s        the Mac's time to simulate a second, for the relative
//                   cost (a DS's is in the DS sweep)
#include <physics_sweep.h>
#include <ragdoll_physics.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Quality {
  double worstStretch = 0, meanWorstStretch = 0;
  double restingSpeed = 0; // mean over shoves, m/s
  double asleepMean = 0, asleepWorst = 0;
  int neverAsleep = 0;
  double macMillisecondsPerSecond = 0;
};

Quality measure(int stepsPerSecond, int subSteps, int shoves, double seconds) {
  Quality quality;
  const double dt = 1.0 / stepsPerSecond;
  const int steps = static_cast<int>(std::lround(seconds * stepsPerSecond));
  int asleepCount = 0;
  double macSeconds = 0;
  for (int seed = 1; seed <= shoves; ++seed) {
    auto model = std::make_unique<ragdoll::Ragdoll>();
    model->world.setSubSteps(static_cast<unsigned>(subSteps));
    model->world.getResolver().setSweeps(16, 4, 4);
    cyclone::Random random(static_cast<unsigned>(seed));
    model->reset(random);
    const auto allAsleep = [&] {
      return std::ranges::none_of(model->bones,
                                  [](const ragdoll::Bone &bone) { return bone.body->getAwake(); });
    };
    double worst = 0;
    // The fastest bone each step of the last second awake
    std::vector<double> lastSecond;
    double asleepAt = -1;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < steps && asleepAt < 0; ++i) {
      model->step(dt);
      worst = std::max(worst, model->worstStretch());
      double fastest = 0;
      for (const ragdoll::Bone &bone : model->bones)
        fastest = std::max<double>(fastest, bone.body->getVelocity().magnitude());
      lastSecond.push_back(fastest);
      if (static_cast<int>(lastSecond.size()) > stepsPerSecond)
        lastSecond.erase(lastSecond.begin());
      if (allAsleep())
        asleepAt = (i + 1) * dt;
    }
    macSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    quality.worstStretch = std::max(quality.worstStretch, worst);
    quality.meanWorstStretch += worst / shoves;
    double resting = 0;
    for (const double speed : lastSecond)
      resting += speed / static_cast<double>(lastSecond.size());
    quality.restingSpeed += resting / shoves;
    if (asleepAt < 0) {
      ++quality.neverAsleep;
    } else {
      ++asleepCount;
      quality.asleepMean += asleepAt;
      quality.asleepWorst = std::max(quality.asleepWorst, asleepAt);
    }
  }
  if (asleepCount > 0)
    quality.asleepMean /= asleepCount;
  // (time simulated: each shove until it fell asleep, at most `seconds`;
  // asleep, a step costs nothing, so per second of its whole run)
  quality.macMillisecondsPerSecond = 1000 * macSeconds / (shoves * seconds);
  return quality;
}

} // namespace

int main(int argc, char **argv) {
  int shoves = 20;
  double seconds = 12;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (std::strcmp(argv[i], "--shoves") == 0) {
      shoves = std::max(1, std::atoi(argv[i + 1]));
    } else if (std::strcmp(argv[i], "--seconds") == 0) {
      seconds = std::max(1.0, std::atof(argv[i + 1]));
    } else {
      std::fprintf(stderr, "usage: ragdoll_sweep [--shoves N] [--seconds S]\n");
      return 2;
    }
  }
  std::printf("Ragdoll sweep: %d shoves, %.0f s each (or until asleep), sweeps 16/4/4\n\n", shoves,
              seconds);
  std::printf("| Steps/s | Sub-steps | Worst stretch (cm) | Mean worst (cm) | Resting speed (cm/s) "
              "| Asleep, mean (s) | Asleep, worst (s) | Never asleep | Mac ms/s |\n");
  std::printf("| --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
  for (const int steps : physics_sweep::kStepsPerSecond) {
    for (const int subSteps : physics_sweep::kSubSteps) {
      const Quality q = measure(steps, subSteps, shoves, seconds);
      // (– where no shove fell asleep)
      char asleepMean[16] = "–", asleepWorst[16] = "–";
      if (q.neverAsleep < shoves) {
        std::snprintf(asleepMean, sizeof asleepMean, "%.1f", q.asleepMean);
        std::snprintf(asleepWorst, sizeof asleepWorst, "%.1f", q.asleepWorst);
      }
      std::printf("| %d | %d | %.2f | %.2f | %.2f | %s | %s | %d | %.1f |\n", steps, subSteps,
                  100 * q.worstStretch, 100 * q.meanWorstStretch, 100 * q.restingSpeed, asleepMean,
                  asleepWorst, q.neverAsleep, q.macMillisecondsPerSecond);
      std::fflush(stdout);
    }
  }
  return 0;
}
