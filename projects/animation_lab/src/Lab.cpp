#include <AnimationLab/Lab.hpp>

#include <SeededRandom/Generator.hpp>

#include <cyclone/particle.h>
#include <cyclone/pfgen.h>
#include <cyclone/world.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace AnimationLab {

namespace {
using cyclone::real;
using cyclone::Vector3;

Vector3 vectorOf(const Point &p, bool flat) {
  return {static_cast<real>(p[0]), static_cast<real>(p[1]),
          flat ? real(0) : static_cast<real>(p[2])};
}
Point pointOf(const Vector3 &v) {
  return {static_cast<float>(static_cast<double>(v.x)),
          static_cast<float>(static_cast<double>(v.y)),
          static_cast<float>(static_cast<double>(v.z))};
}
double distance(const Point &a, const Point &b) {
  return std::hypot(double(a[0]) - b[0], double(a[1]) - b[1], double(a[2]) - b[2]);
}

// A thrown particle flown for `seconds` (a step at a time, recorded), with
// air drag and a floor to bounce on
struct Flight {
  double drag = 1;     // (speed kept each second: 1 none)
  double floor = -1e9; // (none)
  double bounce = 0.5;
};
Recording fly(const Throw &goal, const Vector3 &velocity, const Flight &flight) {
  cyclone::Particle particle;
  particle.setMass(1);
  particle.setPosition(vectorOf(goal.from, goal.flat));
  particle.setVelocity(velocity);
  particle.setAcceleration(0, static_cast<real>(-goal.gravity), 0);
  particle.setDamping(static_cast<real>(flight.drag));
  Recording out;
  out.step = goal.step;
  out.positions.push_back(pointOf(particle.getPosition()));
  const auto steps = static_cast<int>(std::lround(goal.seconds / goal.step));
  for (int i = 0; i < steps; ++i) {
    particle.integrate(static_cast<real>(goal.step));
    // (the floor: back up off it, its downward speed turned up, less)
    Vector3 at = particle.getPosition();
    if (static_cast<double>(at.y) < flight.floor) {
      at.y = static_cast<real>(flight.floor);
      particle.setPosition(at);
      Vector3 v = particle.getVelocity();
      v.y = static_cast<real>(-static_cast<double>(v.y) * flight.bounce);
      particle.setVelocity(v);
    }
    out.positions.push_back(pointOf(particle.getPosition()));
  }
  return out;
}

// The throw that lands at `to` in a vacuum: from s = ut + at^2/2
Vector3 solvedVelocity(const Throw &goal) {
  const double t = goal.seconds;
  const Vector3 from = vectorOf(goal.from, goal.flat), to = vectorOf(goal.to, goal.flat);
  return {static_cast<real>((static_cast<double>(to.x) - static_cast<double>(from.x)) / t),
          static_cast<real>((static_cast<double>(to.y) - static_cast<double>(from.y)) / t +
                            0.5 * goal.gravity * t),
          static_cast<real>((static_cast<double>(to.z) - static_cast<double>(from.z)) / t)};
}
} // namespace

double Recording::seconds() const {
  return positions.empty() ? 0 : step * static_cast<double>(positions.size() - 1);
}

Recording solveThrow(const Throw &goal) {
  // The formula's throw, flown; the steps' small error (the integrator's)
  // corrected by aiming again a couple of times by what it missed by
  Vector3 velocity = solvedVelocity(goal);
  Recording flown = fly(goal, velocity, {});
  for (int again = 0; again < 3; ++again) {
    const Vector3 miss = vectorOf(goal.to, goal.flat) - vectorOf(flown.positions.back(), goal.flat);
    velocity += miss * static_cast<real>(1 / goal.seconds);
    flown = fly(goal, velocity, {});
  }
  return flown;
}

Recording guideAlong(const Guide &guide) {
  Recording out;
  out.step = guide.step;
  if (guide.keys.empty())
    return out;
  // The guide: straight from key to key, held at the last
  const auto guideAt = [&](double t) {
    const auto next = std::find_if(guide.keys.begin(), guide.keys.end(),
                                   [t](const Key &k) { return k.time > t; });
    if (next == guide.keys.begin())
      return guide.keys.front().at;
    if (next == guide.keys.end())
      return guide.keys.back().at;
    const Key &a = *(next - 1), &b = *next;
    const auto u = static_cast<float>((t - a.time) / (b.time - a.time));
    return Point{a.at[0] + (b.at[0] - a.at[0]) * u, a.at[1] + (b.at[1] - a.at[1]) * u,
                 a.at[2] + (b.at[2] - a.at[2]) * u};
  };
  cyclone::Particle particle;
  particle.setMass(1);
  particle.setDamping(1);
  particle.setPosition(vectorOf(guide.keys.front().at, guide.flat));
  // (Cyclone's fake spring: the exact damped spring's motion, stable however
  // stiff; it pulls towards the anchor, moved along the guide each step)
  Vector3 anchor = particle.getPosition();
  cyclone::ParticleFakeSpring spring(&anchor, static_cast<real>(guide.stiffness),
                                     static_cast<real>(guide.damping));
  const double end = guide.keys.back().time + guide.settle;
  out.positions.push_back(pointOf(particle.getPosition()));
  for (double t = guide.step; t <= end + guide.step / 2; t += guide.step) {
    anchor = vectorOf(guideAt(t), guide.flat);
    particle.clearAccumulator();
    spring.updateForce(&particle, static_cast<real>(guide.step));
    particle.integrate(static_cast<real>(guide.step));
    out.positions.push_back(pointOf(particle.getPosition()));
  }
  return out;
}

Found searchThrow(const Search &search) {
  // The cross-entropy method: throws tried around a guess, the best few
  // kept, the guess moved to them and narrowed, round after round
  const Flight flight{search.drag, search.floor, search.bounce};
  const auto missOf = [&](const Recording &r) {
    return distance(r.positions.back(), search.goal.to);
  };
  SeededRandom::Generator random(search.seed);
  const auto gaussian = [&random]() { // (Box-Muller, from the generator)
    const double u1 = (random.below(1u << 24) + 1.0) / ((1u << 24) + 1.0);
    const double u2 = random.below(1u << 24) / double(1u << 24);
    return std::sqrt(-2 * std::log(u1)) * std::cos(2 * 3.14159265358979 * u2);
  };
  Vector3 mean = solvedVelocity(search.goal); // (a vacuum's throw: a start)
  double spread = 0.5 * std::max(1.0, static_cast<double>(mean.magnitude()));
  Found best{fly(search.goal, mean, flight), 0};
  best.miss = missOf(best.recording);
  Vector3 bestVelocity = mean;
  for (int round = 0; round < search.rounds; ++round) {
    std::vector<std::pair<double, Vector3>> tried;
    for (int i = 0; i < search.triesARound; ++i) {
      Vector3 v =
          mean + Vector3(static_cast<real>(gaussian() * spread),
                         static_cast<real>(gaussian() * spread),
                         search.goal.flat ? real(0) : static_cast<real>(gaussian() * spread));
      const Recording flown = fly(search.goal, v, flight);
      const double miss = missOf(flown);
      tried.emplace_back(miss, v);
      if (miss < best.miss) {
        best = {flown, miss};
        bestVelocity = v;
      }
    }
    // (the best fifth: their middle the next guess, their spread the next
    // spread)
    std::sort(tried.begin(), tried.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    const std::size_t keep = std::max<std::size_t>(2, tried.size() / 5);
    Vector3 sum;
    for (std::size_t i = 0; i < keep; ++i)
      sum += tried[i].second;
    mean = sum * static_cast<real>(1.0 / static_cast<double>(keep));
    double var = 0;
    for (std::size_t i = 0; i < keep; ++i)
      var += static_cast<double>((tried[i].second - mean).squareMagnitude());
    spread = std::max(1e-4, std::sqrt(var / static_cast<double>(keep) / 3));
  }
  // Then polished: a small step each way along each axis from the best,
  // kept if it's better, the step halved when none is (a bounce makes the
  // misses jump about: this finds the bottom of the dip it's in)
  double step = 0.1 * std::max(1.0, static_cast<double>(bestVelocity.magnitude()));
  for (int tries = 0; tries < 400 && step > 1e-5; ++tries) {
    bool better = false;
    for (int axis = 0; axis < (search.goal.flat ? 2 : 3) && !better; ++axis)
      for (const double sign : {1.0, -1.0}) {
        Vector3 v = bestVelocity;
        v[static_cast<unsigned>(axis)] += static_cast<real>(sign * step);
        const Recording flown = fly(search.goal, v, flight);
        if (const double miss = missOf(flown); miss < best.miss) {
          best = {flown, miss};
          bestVelocity = v;
          better = true;
          break;
        }
      }
    if (!better)
      step /= 2;
  }
  return best;
}

std::vector<Point> hullCorners(const std::vector<Point> &points) {
  // The point furthest along each of the 26 directions to a cube's faces,
  // edges and corners: each a corner of the hull, the hull close enough
  std::vector<Point> corners;
  for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
      for (int z = -1; z <= 1; ++z) {
        if (x == 0 && y == 0 && z == 0)
          continue;
        const Point *best = nullptr;
        float furthest = -1e30f;
        for (const Point &p : points)
          if (const float along = p[0] * x + p[1] * y + p[2] * z; along > furthest) {
            furthest = along;
            best = &p;
          }
        if (best && std::find(corners.begin(), corners.end(), *best) == corners.end())
          corners.push_back(*best);
      }
  return corners;
}

Recording tumbleThrow(const Tumble &tumble) {
  const Throw &goal = tumble.goal;
  cyclone::World world(cyclone::World::Settings{.maxContacts = 64,
                                                .subSteps = 4,
                                                .friction = static_cast<real>(tumble.friction),
                                                .restitution = static_cast<real>(tumble.bounce),
                                                .iterations = 256});
  cyclone::CollisionPlane floor;
  floor.direction = Vector3(0, 1, 0);
  floor.offset = static_cast<real>(tumble.floor);
  world.addPlane(floor);
  cyclone::RigidBody body;
  cyclone::CollisionHull hull;
  hull.body = &body;
  for (const Point &corner : tumble.corners)
    hull.corners.push_back(vectorOf(corner, false));
  // (its mass 1, turning as a box its hull's size would)
  Vector3 low(1e9, 1e9, 1e9), high(-1e9, -1e9, -1e9);
  for (const Vector3 &c : hull.corners)
    for (unsigned a = 0; a < 3; ++a) {
      low[a] = std::min(low[a], c[a]);
      high[a] = std::max(high[a], c[a]);
    }
  body.setMass(1);
  cyclone::Matrix3 inertia;
  inertia.setBlockInertiaTensor((high - low) * static_cast<real>(0.5), 1);
  body.setInertiaTensor(inertia);
  body.setDamping(static_cast<real>(0.999), static_cast<real>(0.95));
  body.setPosition(vectorOf(goal.from, goal.flat));
  body.setVelocity(solvedVelocity(goal));
  body.setRotation(vectorOf(tumble.spin, goal.flat));
  body.setAcceleration(Vector3(0, static_cast<real>(-goal.gravity), 0));
  body.setCanSleep(false);
  body.calculateDerivedData();
  world.addBody(&body);
  world.addShape(&hull);
  Recording out;
  out.step = goal.step;
  const auto record = [&] {
    out.positions.push_back(pointOf(body.getPosition()));
    const cyclone::Quaternion q = body.getOrientation();
    out.turns.push_back({static_cast<float>(static_cast<double>(q.r)),
                         static_cast<float>(static_cast<double>(q.i)),
                         static_cast<float>(static_cast<double>(q.j)),
                         static_cast<float>(static_cast<double>(q.k))});
  };
  record();
  // (on until it's been still a moment, after its throw's time; or the
  // settling time's up)
  constexpr double kStill = 0.05, kStillFor = 0.3;
  double still = 0;
  for (double t = goal.step; t <= goal.seconds + tumble.settle; t += goal.step) {
    world.startFrame();
    world.runPhysics(static_cast<real>(goal.step));
    record();
    const bool moving = static_cast<double>(body.getVelocity().magnitude()) > kStill ||
                        static_cast<double>(body.getRotation().magnitude()) > kStill;
    still = moving ? 0 : still + goal.step;
    if (t > goal.seconds && still >= kStillFor)
      break;
  }
  return out;
}

ScratchPadd::Motion::BakedMotion bake(const Recording &recording, std::size_t pathPoints,
                                      std::size_t timingPoints) {
  ScratchPadd::Motion::BakedMotion baked;
  baked.seconds = static_cast<float>(recording.seconds());
  const std::vector<Point> &p = recording.positions;
  if (p.empty())
    return baked;
  // The way it went: how far it had come at each step
  std::vector<double> came(p.size(), 0);
  for (std::size_t i = 1; i < p.size(); ++i)
    came[i] = came[i - 1] + distance(p[i - 1], p[i]);
  const double total = came.back();
  if (total <= 0) {
    baked.path = {p.front()};
    baked.timing = {0};
    return baked;
  }
  // The path: points evenly spaced along it (each where that far along was
  // reached, between two steps)
  pathPoints = std::max<std::size_t>(pathPoints, 2);
  std::size_t j = 1;
  for (std::size_t k = 0; k < pathPoints; ++k) {
    const double want = total * static_cast<double>(k) / static_cast<double>(pathPoints - 1);
    while (j + 1 < p.size() && came[j] < want)
      ++j;
    const double span = came[j] - came[j - 1];
    const auto u =
        static_cast<float>(span > 0 ? std::clamp((want - came[j - 1]) / span, 0.0, 1.0) : 0);
    baked.path.push_back({p[j - 1][0] + (p[j][0] - p[j - 1][0]) * u,
                          p[j - 1][1] + (p[j][1] - p[j - 1][1]) * u,
                          p[j - 1][2] + (p[j][2] - p[j - 1][2]) * u});
  }
  // The timing: how far along at evenly spaced moments
  timingPoints = std::max<std::size_t>(timingPoints, 2);
  for (std::size_t m = 0; m < timingPoints; ++m) {
    const double place = static_cast<double>(p.size() - 1) * static_cast<double>(m) /
                         static_cast<double>(timingPoints - 1);
    const auto i = std::min(static_cast<std::size_t>(place), p.size() - 2);
    const double u = place - static_cast<double>(i);
    baked.timing.push_back(static_cast<float>((came[i] + (came[i + 1] - came[i]) * u) / total));
    // (and how it's turned then, for a thing that tumbles)
    if (recording.turns.size() == p.size())
      baked.turns.push_back(recording.turns[u < 0.5 ? i : i + 1]);
  }
  return baked;
}

double bakeError(const Recording &recording, const ScratchPadd::Motion::BakedMotion &baked) {
  double worst = 0;
  for (std::size_t i = 0; i < recording.positions.size(); ++i)
    worst = std::max(
        worst, distance(recording.positions[i],
                        baked.at(static_cast<float>(recording.step * static_cast<double>(i)))));
  return worst;
}

std::string writeText(const ScratchPadd::Motion::BakedMotion &baked) {
  std::ostringstream out;
  out.precision(7);
  out << "seconds " << baked.seconds << "\n";
  for (const Point &p : baked.path)
    out << "path " << p[0] << " " << p[1] << " " << p[2] << "\n";
  out << "timing";
  for (const float f : baked.timing)
    out << " " << f;
  out << "\n";
  for (const auto &turn : baked.turns)
    out << "turn " << turn[0] << " " << turn[1] << " " << turn[2] << " " << turn[3] << "\n";
  return out.str();
}

std::optional<ScratchPadd::Motion::BakedMotion> readText(const std::string &text) {
  ScratchPadd::Motion::BakedMotion baked;
  std::istringstream in(text);
  std::string word;
  bool sawSeconds = false;
  while (in >> word) {
    if (word == "seconds") {
      if (!(in >> baked.seconds))
        return std::nullopt;
      sawSeconds = true;
    } else if (word == "path") {
      Point p{};
      if (!(in >> p[0] >> p[1] >> p[2]))
        return std::nullopt;
      baked.path.push_back(p);
    } else if (word == "timing") {
      // (the numbers up to the next word)
      float f = 0;
      while (in >> f)
        baked.timing.push_back(f);
      in.clear();
    } else if (word == "turn") {
      ScratchPadd::Motion::BakedMotion::Turn turn{};
      if (!(in >> turn[0] >> turn[1] >> turn[2] >> turn[3]))
        return std::nullopt;
      baked.turns.push_back(turn);
    } else {
      return std::nullopt;
    }
  }
  if (!sawSeconds)
    return std::nullopt;
  return baked;
}

} // namespace AnimationLab
