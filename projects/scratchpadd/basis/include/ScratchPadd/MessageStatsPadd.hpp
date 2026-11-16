// MessageStatsPadd.hpp - the system's message statistics (MessageStats.hpp)
// on a tab and in the log: listing it turns the counting on (it reads them
// with a MessageStats::Reader of its own, so other readers, a graph say,
// get their own counts). Every 5 seconds its tab shows the busiest message
// types and padds over those 5 seconds (padds by time running jobs of every
// kind: handlers, repeat() and pushed work); every minute it logs the
// minute's. A type sent 100 times a second
// or more is marked as a candidate: for a stream if it's trivially copyable
// (a fixed size, copied as bytes), otherwise for pooling (see the README's
// message performance notes). Its Counting switch turns it off and on while the app
// runs.
//
// Every app can list it: it counts from the start in debug builds
// (SAKBOX_DEBUG on the consoles; no NDEBUG on desktops), and in other builds
// it's off, costing one atomic load per message, until its switch turns it
// on. (A padd deriving from it can pass counting to choose.)
//
//   ScratchPadd::SystemImplementation<ScratchPadd::AboutPadd, ScratchPadd::MessageStatsPadd,
//                                     GamePadd, ...> system;   // (in debug builds)
//
// It runs when nothing else needs the processor (Runs::WhenIdle).
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/MessageStats.hpp>

#include <chrono>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd {

class MessageStatsPadd : public Base {
public:
  static constexpr std::chrono::seconds kTabEvery{5};
  static constexpr int kTabsPerLog = 12;           // (a log line a minute)
  static constexpr double kPoolingCandidate = 100; // messages a second
  static constexpr std::size_t kRows = 8;

#if defined(SAKBOX_DEBUG) || (!defined(NDEBUG) && !defined(__NDS__) && !defined(__3DS__))
  static constexpr bool kCountingByDefault = true;
#else
  static constexpr bool kCountingByDefault = false;
#endif

  explicit MessageStatsPadd(System *system, bool counting = kCountingByDefault)
      : Base(system), countingAtStart_(counting) {}
  std::string name() override { return "Message stats"; }
  Runs runs() override { return Runs::WhenIdle; }
  std::optional<std::chrono::duration<double>> repeatInterval() override { return kTabEvery; }
  ControlValueMap generateControls() override {
    return {{kCounting, ControlType::Boolean(countingAtStart_)},
            {kTypes, ControlType::Info("(counting...)")},
            {kPadds, ControlType::Info("")}};
  }
  void controlChanged(const std::string &name, const ControlTypeVariant &) override {
    if (name == kCounting)
      setCounting(control<ControlType::Boolean>(kCounting).value);
  }
  void starting() override { setCounting(countingAtStart_); }
  void finishing() override { setCounting(false); }

  // Every kTabEvery: the tab shows the last 5 seconds; every kTabsPerLog
  // times, the log gets the minute
  void repeat() override {
    if (!reader_)
      return;
    const MessageStats::Snapshot recent = reader_->take();
    setControl(kTypes, ControlType::Info(typeTable(recent)));
    setControl(kPadds, ControlType::Info(paddTable(recent)));
    add(minute_, recent);
    if (++tabs_ >= kTabsPerLog) {
      log(minute_);
      minute_ = {};
      tabs_ = 0;
    }
  }

  // The tables, in lines of tab-separated columns (a panel shows a table)
  static std::string typeTable(const MessageStats::Snapshot &snapshot) {
    std::string table = "Type\tPer s\tJobs per s\tBytes";
    const double seconds = secondsOf(snapshot);
    for (std::size_t i = 0; i < snapshot.types.size() && i < kRows; ++i) {
      const MessageStats::TypeCounts &type = snapshot.types[i];
      table += "\n" + type.name + candidate(type, seconds, " (stream?)", " (pool?)") + "\t" +
               number(type.messages / seconds) + "\t" + number(type.jobs / seconds) + "\t" +
               std::to_string(type.size);
    }
    return table;
  }
  static std::string paddTable(const MessageStats::Snapshot &snapshot) {
    std::string table = "Padd\tJobs per s\tBusy\tIn repeat()\tLongest\tDeepest queue\tLongest wait";
    const double seconds = secondsOf(snapshot);
    for (std::size_t i = 0; i < snapshot.padds.size() && i < kRows; ++i) {
      const MessageStats::PaddCounts &padd = snapshot.padds[i];
      table += "\n" + padd.name + "\t" + number(padd.jobs() / seconds) + "\t" +
               percentOf(padd.busy(), seconds) + "\t" + percentOf(padd.repeats.busy, seconds) +
               "\t" + number(std::chrono::duration<double, std::milli>(padd.longest()).count()) +
               " ms\t" + std::to_string(padd.deepestQueue) + "\t" +
               number(std::chrono::duration<double, std::milli>(padd.longestWait).count()) + " ms";
    }
    return table;
  }

private:
  static constexpr const char *kCounting = "Counting";
  static constexpr const char *kTypes = "Busiest types";
  static constexpr const char *kPadds = "Busiest padds";

  // " (stream?)" or " (pool?)" for a type sent kPoolingCandidate times a
  // second or more (a stream if it's trivially copyable), else nothing
  static std::string candidate(const MessageStats::TypeCounts &type, double seconds,
                               const char *stream, const char *pool) {
    if (type.messages / seconds < kPoolingCandidate)
      return "";
    return type.triviallyCopyable ? stream : pool;
  }
  // Time as a share of `seconds`, e.g. "12%"
  static std::string percentOf(MessageStats::Clock::duration time, double seconds) {
    return number(100.0 * std::chrono::duration<double>(time).count() / seconds) + "%";
  }
  // Adds one kind of job's counts into another's
  static void add(MessageStats::JobCounts &total, const MessageStats::JobCounts &more) {
    total.count += more.count;
    total.busy += more.busy;
    total.longest = std::max(total.longest, more.longest);
  }
  static double secondsOf(const MessageStats::Snapshot &snapshot) {
    const double seconds = std::chrono::duration<double>(snapshot.over).count();
    return seconds > 0 ? seconds : 1;
  }
  static std::string number(double value) {
    char text[32];
    std::snprintf(text, sizeof(text), value < 10 ? "%.1f" : "%.0f", value);
    return text;
  }
  // Adds `more` into `total`, matching types and padds by name
  static void add(MessageStats::Snapshot &total, const MessageStats::Snapshot &more) {
    total.over += more.over;
    for (const MessageStats::TypeCounts &type : more.types) {
      auto found = std::ranges::find(total.types, type.name, &MessageStats::TypeCounts::name);
      if (found == total.types.end()) {
        total.types.push_back(type);
      } else {
        found->messages += type.messages;
        found->jobs += type.jobs;
      }
    }
    for (const MessageStats::PaddCounts &padd : more.padds) {
      auto found = std::ranges::find(total.padds, padd.name, &MessageStats::PaddCounts::name);
      if (found == total.padds.end()) {
        total.padds.push_back(padd);
      } else {
        add(found->messages, padd.messages);
        add(found->repeats, padd.repeats);
        add(found->pushed, padd.pushed);
        found->deepestQueue = std::max(found->deepestQueue, padd.deepestQueue);
        found->longestWait = std::max(found->longestWait, padd.longestWait);
      }
    }
    std::ranges::stable_sort(total.types, std::ranges::greater{}, &MessageStats::TypeCounts::jobs);
    std::ranges::stable_sort(total.padds, std::ranges::greater{}, &MessageStats::PaddCounts::busy);
  }
  void log(const MessageStats::Snapshot &minute) {
    const double seconds = secondsOf(minute);
    std::uint64_t messages = 0, jobs = 0;
    for (const MessageStats::TypeCounts &type : minute.types) {
      messages += type.messages;
      jobs += type.jobs;
    }
    std::string types;
    for (std::size_t i = 0; i < minute.types.size() && i < 4; ++i) {
      const MessageStats::TypeCounts &type = minute.types[i];
      types +=
          (i ? "; " : "") + type.name + " " + number(type.messages / seconds) + "/s" +
          candidate(type, seconds, " (a candidate for a stream)", " (a candidate for pooling)");
    }
    logger().info("Messages over {}s: {} ({}/s), {} jobs. Busiest: {}", number(seconds), messages,
                  number(messages / seconds), jobs, types.empty() ? "none" : types);
    std::string padds;
    for (std::size_t i = 0; i < minute.padds.size() && i < 4; ++i) {
      const MessageStats::PaddCounts &padd = minute.padds[i];
      padds += (i ? "; " : "") + padd.name + " busy " + percentOf(padd.busy(), seconds) +
               " (repeat() " + percentOf(padd.repeats.busy, seconds) + "), longest " +
               number(std::chrono::duration<double, std::milli>(padd.longest()).count()) +
               " ms, deepest queue " + std::to_string(padd.deepestQueue) + ", longest wait " +
               number(std::chrono::duration<double, std::milli>(padd.longestWait).count()) + " ms";
    }
    if (!padds.empty())
      logger().info("Busiest padds: {}", padds);
  }

  // Counting on (a reader of its own) or off (none)
  void setCounting(bool on) {
    if (on && !reader_)
      reader_.emplace(system_->messageStats());
    if (!on)
      reader_.reset();
  }

  bool countingAtStart_;
  std::optional<MessageStats::Reader> reader_;
  MessageStats::Snapshot minute_;
  int tabs_ = 0;
};

} // namespace ScratchPadd
