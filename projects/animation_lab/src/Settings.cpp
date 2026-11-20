#include <AnimationLab/Settings.hpp>

#include <cstdio>
#include <iomanip>
#include <sstream>

namespace AnimationLab {

std::string_view nameOf(Way way) {
  switch (way) {
  case Way::Throw:
    return "Throw";
  case Way::Guide:
    return "Guide";
  case Way::Search:
    return "Search";
  case Way::Tumble:
    return "Tumble";
  }
  return "?";
}

Made make(const Settings &settings, const std::vector<Point> &corners) {
  Made made;
  switch (settings.way) {
  case Way::Throw: {
    Throw goal = settings.throwing;
    goal.flat = settings.flat;
    made.recording = solveThrow(goal);
    break;
  }
  case Way::Guide: {
    Guide guide = settings.guide;
    guide.flat = settings.flat;
    made.recording = guideAlong(guide);
    break;
  }
  case Way::Search: {
    Search search = settings.search;
    search.goal.flat = settings.flat;
    const Found found = searchThrow(search);
    made.recording = found.recording;
    made.miss = found.miss;
    break;
  }
  case Way::Tumble: {
    Tumble tumble = settings.tumble;
    tumble.goal.flat = settings.flat;
    tumble.corners = corners;
    made.recording = tumbleThrow(tumble);
    break;
  }
  }
  made.baked = bake(made.recording, static_cast<std::size_t>(settings.pathPoints),
                    static_cast<std::size_t>(settings.timingPoints));
  made.error = bakeError(made.recording, made.baked);
  return made;
}

namespace {

std::string number(double v) {
  char text[32];
  std::snprintf(text, sizeof text, "%g", v);
  return text;
}
std::string point(const Point &p) { return number(p[0]) + " " + number(p[1]) + " " + number(p[2]); }

} // namespace

std::string summary(const Settings &s) {
  std::string out = std::string(nameOf(s.way)) + " " + s.shape;
  switch (s.way) {
  case Way::Throw:
    out += " " + number(s.throwing.seconds) + "s g" + number(s.throwing.gravity);
    break;
  case Way::Guide:
    out += " " + std::to_string(s.guide.keys.size()) + " keys k" + number(s.guide.stiffness) +
           " d" + number(s.guide.damping);
    break;
  case Way::Search:
    out += " " + number(s.search.goal.seconds) + "s drag" + number(s.search.drag) + " bounce" +
           number(s.search.bounce);
    break;
  case Way::Tumble:
    out += " " + number(s.tumble.goal.seconds) + "s spin " + point(s.tumble.spin) + " bounce" +
           number(s.tumble.bounce);
    break;
  }
  return out;
}

std::string writeSettings(const Settings &s) {
  std::ostringstream out;
  out << "way " << nameOf(s.way) << "\n";
  out << "flat " << (s.flat ? 1 : 0) << "\n";
  out << "shape " << s.shape << "\n";
  out << "size " << number(s.size) << "\n";
  out << "bake " << s.pathPoints << " " << s.timingPoints << "\n";
  const Throw &t = s.throwing;
  out << "throw " << point(t.from) << " " << point(t.to) << " " << number(t.seconds) << " "
      << number(t.gravity) << "\n";
  out << "guide " << number(s.guide.stiffness) << " " << number(s.guide.damping) << " "
      << number(s.guide.settle) << "\n";
  for (const Key &key : s.guide.keys)
    out << "key " << number(key.time) << " " << point(key.at) << "\n";
  const Search &f = s.search;
  out << "search " << point(f.goal.from) << " " << point(f.goal.to) << " " << number(f.goal.seconds)
      << " " << number(f.goal.gravity) << " " << number(f.drag) << " " << number(f.floor) << " "
      << number(f.bounce) << "\n";
  const Tumble &u = s.tumble;
  out << "tumble " << point(u.goal.from) << " " << point(u.goal.to) << " " << number(u.goal.seconds)
      << " " << number(u.goal.gravity) << " " << point(u.spin) << " " << number(u.floor) << " "
      << number(u.bounce) << " " << number(u.friction) << " " << number(u.settle) << "\n";
  return out.str();
}

std::optional<Settings> readSettings(std::string_view text) {
  Settings s;
  std::istringstream lines{std::string(text)};
  std::string line;
  bool any = false, keys = false;
  const auto readPoint = [](std::istream &in, Point &p) {
    return bool(in >> p[0] >> p[1] >> p[2]);
  };
  while (std::getline(lines, line)) {
    std::istringstream in(line);
    std::string word;
    if (!(in >> word))
      continue;
    bool ok = true;
    if (word == "way") {
      std::string way;
      in >> way;
      bool known = false;
      for (const Way w : {Way::Throw, Way::Guide, Way::Search, Way::Tumble})
        if (nameOf(w) == way) {
          s.way = w;
          known = true;
        }
      ok = known;
    } else if (word == "flat") {
      int flat = 0;
      ok = bool(in >> flat);
      s.flat = flat != 0;
    } else if (word == "shape") {
      ok = bool(in >> s.shape);
    } else if (word == "size") {
      ok = bool(in >> s.size);
    } else if (word == "bake") {
      ok = bool(in >> s.pathPoints >> s.timingPoints);
    } else if (word == "throw") {
      Throw &t = s.throwing;
      ok = readPoint(in, t.from) && readPoint(in, t.to) && in >> t.seconds >> t.gravity;
    } else if (word == "guide") {
      ok = bool(in >> s.guide.stiffness >> s.guide.damping >> s.guide.settle);
    } else if (word == "key") {
      if (!keys)
        s.guide.keys.clear(); // (the saved keys, not the defaults)
      keys = true;
      Key key{};
      ok = in >> key.time && readPoint(in, key.at);
      s.guide.keys.push_back(key);
    } else if (word == "search") {
      Search &f = s.search;
      ok = readPoint(in, f.goal.from) && readPoint(in, f.goal.to) &&
           in >> f.goal.seconds >> f.goal.gravity >> f.drag >> f.floor >> f.bounce;
    } else if (word == "tumble") {
      Tumble &u = s.tumble;
      ok = readPoint(in, u.goal.from) && readPoint(in, u.goal.to) &&
           in >> u.goal.seconds >> u.goal.gravity && readPoint(in, u.spin) &&
           in >> u.floor >> u.bounce >> u.friction >> u.settle;
    } else {
      ok = false;
    }
    if (!ok)
      return std::nullopt;
    any = true;
  }
  if (!any)
    return std::nullopt;
  return s;
}

} // namespace AnimationLab
