// ActionLog.hpp - a game as its starting state and the list of what
// happened, written as text: saves, replays, recorded games to analyse,
// network sync and bots all build on it.
//
// A game's rules are a function from (state, action) to the next state.
// Every chance outcome is an action too, by "the dealer" (a roll, a tile
// flipped, a card dealt), so the log holds everything that happened and a
// replay needs no random numbers: the seed makes the draws while playing,
// and only the live game (or a network game's host) ever has it.
//
// A game says what it is with a struct of static functions (see the Game
// concept below). Then:
//
//   ActionLog::Play<CosmicHex> play(startingBoard);
//   play.play(Roll{3, 5});                   // false if the rules refuse it
//   std::string text = ActionLog::write(play.log());
//   ...
//   auto log = ActionLog::read<CosmicHex>(text);        // or an error
//   auto result = ActionLog::replay(*log);               // the game again, checked
//
// The text: a header, the starting state, then one action a line, with a
// fingerprint of the state every so often, so a replay that comes out
// different (a changed rule, two devices out of step) is caught where it
// first differs:
//
//   action_log 1
//   game cosmic_hex 3
//   meta players Ann, Bot 1
//   start 2
//   <the game's own lines for its starting state>
//   <...>
//   a roll 3 5
//   a move 2 3,-1
//   f 2 9c1f0e2a7b33d401
//
// Lines starting with # are comments (notes added when analysing).
#pragma once

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ActionLog {

// What a game provides, as static members of a struct:
//   kName           its name in logs (no spaces), "cosmic_hex"
//   kRulesVersion   bumped whenever a rule changes what an action does
//   State, Action   its state and its actions (the dealer's included)
//   apply(state, action)   plays an action, or returns false (changing
//                          nothing) if the rules don't allow it
//   writeState / readState     the state as text (any number of lines)
//   writeAction / readAction   an action as text (one line)
// The read functions return nothing for text that isn't one.
template <typename G>
concept Game = requires(typename G::State &state, const typename G::State &constState,
                        const typename G::Action &action, std::string_view text) {
  { G::kName } -> std::convertible_to<std::string_view>;
  { G::kRulesVersion } -> std::convertible_to<int>;
  { G::apply(state, action) } -> std::same_as<bool>;
  { G::writeState(constState) } -> std::same_as<std::string>;
  { G::readState(text) } -> std::same_as<std::optional<typename G::State>>;
  { G::writeAction(action) } -> std::same_as<std::string>;
  { G::readAction(text) } -> std::same_as<std::optional<typename G::Action>>;
};

// A state's fingerprint: FNV-1a of its text, so two states with the same
// text have the same one, on every platform.
template <Game G> std::uint64_t fingerprint(const typename G::State &state) {
  std::uint64_t hash = 0xcbf29ce484222325ull;
  for (char c : G::writeState(state)) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 0x100000001b3ull;
  }
  return hash;
}

template <Game G> struct Log {
  struct Fingerprint {
    std::size_t afterActions = 0; // the state's after this many actions
    std::uint64_t value = 0;
    bool operator==(const Fingerprint &) const = default;
  };

  int rulesVersion = G::kRulesVersion;
  std::vector<std::pair<std::string, std::string>> meta; // (key, value), in order
  typename G::State start;
  std::vector<typename G::Action> actions;
  std::vector<Fingerprint> fingerprints;
};

// Plays a game, recording what's played.
template <Game G> class Play {
public:
  // `fingerprintEvery`: a fingerprint after every that many actions (0: none)
  explicit Play(typename G::State start, std::size_t fingerprintEvery = 10)
      : state_(start), every_(fingerprintEvery) {
    log_.start = std::move(start);
  }

  // Plays an action and records it, or returns false (recording nothing) if
  // the rules don't allow it.
  bool play(const typename G::Action &action) {
    if (!G::apply(state_, action))
      return false;
    log_.actions.push_back(action);
    if (every_ != 0 && log_.actions.size() % every_ == 0)
      log_.fingerprints.push_back({log_.actions.size(), fingerprint<G>(state_)});
    return true;
  }

  // Notes for the log's header (players' names, where it was played...)
  void addMeta(std::string key, std::string value) {
    log_.meta.emplace_back(std::move(key), std::move(value));
  }

  const typename G::State &state() const { return state_; }
  const Log<G> &log() const { return log_; }

private:
  typename G::State state_;
  std::size_t every_;
  Log<G> log_;
};

// What went wrong reading or replaying a log: the line (1 is the first;
// 0 when not tied to one) and why.
struct Problem {
  std::size_t line = 0;
  std::string why;
};

namespace detail {
inline std::string hex64(std::uint64_t value) {
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i, value >>= 4)
    out[static_cast<std::size_t>(i)] = "0123456789abcdef"[value & 15];
  return out;
}

inline std::vector<std::string_view> lines(std::string_view text) {
  std::vector<std::string_view> out;
  while (!text.empty()) {
    const std::size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
    out.push_back(line);
    if (end == std::string_view::npos)
      break;
    text.remove_prefix(end + 1);
  }
  return out;
}

template <typename T> std::optional<T> number(std::string_view text, int base = 10) {
  T value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
  if (error != std::errc{} || end != text.data() + text.size())
    return std::nullopt;
  return value;
}
} // namespace detail

inline constexpr int kFormatVersion = 1;

// The log as text.
template <Game G> std::string write(const Log<G> &log) {
  std::string out = "action_log " + std::to_string(kFormatVersion) + "\n";
  out += "game " + std::string(G::kName) + " " + std::to_string(log.rulesVersion) + "\n";
  for (const auto &[key, value] : log.meta)
    ((((out += "meta ") += key) += ' ') += value) += '\n';
  const std::string state = G::writeState(log.start);
  const std::vector<std::string_view> stateLines = detail::lines(state);
  out += "start " + std::to_string(stateLines.size()) + "\n";
  for (std::string_view line : stateLines)
    (out += line) += '\n';
  std::size_t nextFingerprint = 0;
  for (std::size_t i = 0; i < log.actions.size(); ++i) {
    out += "a " + G::writeAction(log.actions[i]) + "\n";
    while (nextFingerprint < log.fingerprints.size() &&
           log.fingerprints[nextFingerprint].afterActions == i + 1) {
      out += "f " + std::to_string(i + 1) + " " +
             detail::hex64(log.fingerprints[nextFingerprint].value) + "\n";
      ++nextFingerprint;
    }
  }
  return out;
}

// A log from its text, or what's wrong with it. It checks the text is a log
// of this game and that every action reads; whether the actions are legal
// is replay()'s job.
template <Game G> std::expected<Log<G>, Problem> read(std::string_view text) {
  const std::vector<std::string_view> lines = detail::lines(text);
  const auto fail = [](std::size_t line, std::string why) {
    return std::unexpected(Problem{line, std::move(why)});
  };
  std::size_t at = 0; // the index of the next line
  const auto next = [&]() -> std::optional<std::string_view> {
    while (at < lines.size() && (lines[at].empty() || lines[at].front() == '#'))
      ++at;
    if (at == lines.size())
      return std::nullopt;
    return lines[at++];
  };
  Log<G> log;

  const auto header = next();
  if (!header || *header != "action_log " + std::to_string(kFormatVersion))
    return fail(at, "not an action log (or a format this version can't read)");
  const auto game = next();
  const std::string gamePrefix = "game " + std::string(G::kName) + " ";
  if (!game || !game->starts_with(gamePrefix))
    return fail(at, "not a log of " + std::string(G::kName));
  const auto version = detail::number<int>(game->substr(gamePrefix.size()));
  if (!version)
    return fail(at, "no rules version");
  log.rulesVersion = *version;

  std::optional<std::string_view> line = next();
  while (line && line->starts_with("meta ")) {
    const std::string_view rest = line->substr(5);
    const std::size_t space = rest.find(' ');
    log.meta.emplace_back(std::string(rest.substr(0, space)),
                          space == std::string_view::npos ? ""
                                                          : std::string(rest.substr(space + 1)));
    line = next();
  }

  if (!line || !line->starts_with("start "))
    return fail(at, "no starting state");
  const auto count = detail::number<std::size_t>(line->substr(6));
  if (!count || at + *count > lines.size())
    return fail(at, "the starting state's line count is wrong");
  const std::size_t startLine = at + 1;
  std::string stateText;
  for (std::size_t i = 0; i < *count; ++i) // the state's own lines, taken as they are
    (stateText += lines[at++]) += '\n';
  auto start = G::readState(stateText);
  if (!start)
    return fail(startLine, "the starting state doesn't read");
  log.start = std::move(*start);

  while ((line = next())) {
    if (line->starts_with("a ")) {
      auto action = G::readAction(line->substr(2));
      if (!action)
        return fail(at, "an action that doesn't read: " + std::string(line->substr(2)));
      log.actions.push_back(std::move(*action));
    } else if (line->starts_with("f ")) {
      const std::string_view rest = line->substr(2);
      const std::size_t space = rest.find(' ');
      const auto after = detail::number<std::size_t>(rest.substr(0, space));
      const auto value = space == std::string_view::npos
                             ? std::nullopt
                             : detail::number<std::uint64_t>(rest.substr(space + 1), 16);
      if (!after || !value || *after != log.actions.size())
        return fail(at, "a fingerprint line that doesn't fit");
      log.fingerprints.push_back({*after, *value});
    } else {
      return fail(at, "a line that isn't an action or a fingerprint");
    }
  }
  return log;
}

template <Game G> struct Replay {
  typename G::State state;        // after the last action that played
  std::size_t played = 0;         // how many actions played
  std::optional<Problem> problem; // why it stopped early, if it did (line 0: see why)
  bool ok() const { return !problem; }
};

// Plays a log's actions from its start, checking as it goes: the rules
// version, that every action is allowed, and every fingerprint. Stops at the
// first problem, with the state as it was then. `onAction(state, action,
// index)` (optional) sees each action after it plays, for analysis.
template <Game G, typename OnAction> Replay<G> replay(const Log<G> &log, OnAction &&onAction) {
  Replay<G> result{log.start, 0, std::nullopt};
  if (log.rulesVersion != G::kRulesVersion) {
    result.problem = Problem{0, "made with rules version " + std::to_string(log.rulesVersion) +
                                    ", these are version " + std::to_string(G::kRulesVersion)};
    return result;
  }
  std::size_t nextFingerprint = 0;
  for (std::size_t i = 0; i < log.actions.size(); ++i) {
    if (!G::apply(result.state, log.actions[i])) {
      result.problem = Problem{0, "action " + std::to_string(i + 1) +
                                      " isn't allowed: " + G::writeAction(log.actions[i])};
      return result;
    }
    result.played = i + 1;
    onAction(std::as_const(result.state), log.actions[i], i);
    while (nextFingerprint < log.fingerprints.size() &&
           log.fingerprints[nextFingerprint].afterActions == i + 1) {
      if (fingerprint<G>(result.state) != log.fingerprints[nextFingerprint].value) {
        result.problem =
            Problem{0, "the game differs from the recording after action " + std::to_string(i + 1)};
        return result;
      }
      ++nextFingerprint;
    }
  }
  return result;
}

template <Game G> Replay<G> replay(const Log<G> &log) {
  return replay(log, [](const typename G::State &, const typename G::Action &, std::size_t) {});
}

} // namespace ActionLog
