// MessageStatsPadd.hpp - the system's message statistics (MessageStats.hpp)
// on a tab and in the log: listing it turns the counting on. Every 5
// seconds its tab shows the busiest message types and padds over those 5
// seconds; every minute it logs the minute's. A type sent 100 times a second
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
      system_->messageStats().setEnabled(control<ControlType::Boolean>(kCounting).value);
  }
  void starting() override { system_->messageStats().setEnabled(countingAtStart_); }
  void finishing() override { system_->messageStats().setEnabled(false); }

  // Every kTabEvery: the tab shows the last 5 seconds; every kTabsPerLog
  // times, the log gets the minute
  void repeat() override {
    MessageStats &stats = system_->messageStats();
    if (!stats.enabled())
      return;
    const MessageStats::Snapshot recent = stats.take();
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
    std::string table = "Padd\tHandled per s\tBusy\tLongest\tDeepest queue\tLongest wait";
    const double seconds = secondsOf(snapshot);
    for (std::size_t i = 0; i < snapshot.padds.size() && i < kRows; ++i) {
      const MessageStats::PaddCounts &padd = snapshot.padds[i];
      table += "\n" + padd.name + "\t" + number(padd.handled / seconds) + "\t" +
               number(100.0 * std::chrono::duration<double>(padd.busy).count() / seconds) + "%\t" +
               number(std::chrono::duration<double, std::milli>(padd.longest).count()) + " ms\t" +
               std::to_string(padd.deepestQueue) + "\t" +
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
        found->handled += padd.handled;
        found->busy += padd.busy;
        found->longest = std::max(found->longest, padd.longest);
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
      padds += (i ? "; " : "") + padd.name + " busy " +
               number(100.0 * std::chrono::duration<double>(padd.busy).count() / seconds) +
               "%, longest " +
               number(std::chrono::duration<double, std::milli>(padd.longest).count()) +
               " ms, deepest queue " + std::to_string(padd.deepestQueue) + ", longest wait " +
               number(std::chrono::duration<double, std::milli>(padd.longestWait).count()) + " ms";
    }
    if (!padds.empty())
      logger().info("Busiest padds: {}", padds);
  }

  bool countingAtStart_;
  MessageStats::Snapshot minute_;
  int tabs_ = 0;
};

} // namespace ScratchPadd
