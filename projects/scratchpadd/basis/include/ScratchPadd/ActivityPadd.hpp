// ActivityPadd.hpp - what the app is doing, as graphs on a tab: every second,
// the messages sent that second, and for each padd how busy it was (its
// share of the second spent running jobs: message handlers, repeat() and
// pushed work) and the longest a job waited for it. Each graph keeps the
// last minute. A padd's graphs appear the first second it runs a job.
//
// It reads MessageStats with a Reader of its own (MessageStats.hpp), so it
// counts whatever else reads them (MessageStatsPadd's tables, say), and
// listing it turns the counting on, at the cost of a lock per message and
// job: for debug builds and tools.
//
//   ScratchPadd::SystemImplementation<ScratchPadd::AboutPadd, ScratchPadd::ActivityPadd,
//                                     GamePadd, ...> system;
//
// "Busy" is time running jobs, which includes any waiting a job does
// itself: a padd whose frame job waits for the screen to refresh shows as
// busy for that wait. It runs when nothing else needs the processor
// (Runs::WhenIdle).
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/MessageStats.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ScratchPadd {

class ActivityPadd : public Base {
public:
  static constexpr std::chrono::seconds kEvery{1};
  static constexpr std::size_t kHistory = 60; // a minute of seconds
  static constexpr const char *kMessages = "Messages a second";

  explicit ActivityPadd(System *system) : Base(system) {}
  std::string name() override { return "Activity"; }
  Runs runs() override { return Runs::WhenIdle; }
  std::optional<std::chrono::duration<double>> repeatInterval() override { return kEvery; }
  ControlValueMap generateControls() override {
    return {{kMessages, ControlType::Graph(kHistory, ControlType::Graph::Style::Line)}};
  }
  void starting() override { reader_.emplace(system_->messageStats()); }
  void finishing() override { reader_.reset(); }

  // The names of a padd's graphs
  static std::string busyGraph(const std::string &padd) { return padd + " busy"; }
  static std::string waitGraph(const std::string &padd) { return padd + " longest wait"; }

  // Every second: the second's counts, one value added to every graph
  void repeat() override {
    if (!reader_)
      return;
    const MessageStats::Snapshot second = reader_->take();
    const double seconds = std::chrono::duration<double>(second.over).count();
    if (seconds <= 0)
      return;

    std::vector<std::pair<std::string, float>> values;
    std::uint64_t messages = 0;
    for (const MessageStats::TypeCounts &type : second.types)
      messages += type.messages;
    values.emplace_back(kMessages, static_cast<float>(messages / seconds));

    addGraphsForNewPadds(second);
    std::set<std::string> ranThisSecond;
    for (const MessageStats::PaddCounts &padd : second.padds) {
      ranThisSecond.insert(padd.name);
      const double busy = std::chrono::duration<double>(padd.busy()).count() / seconds;
      const double waitMs = std::chrono::duration<double, std::milli>(padd.longestWait).count();
      values.emplace_back(busyGraph(padd.name), static_cast<float>(100.0 * busy));
      values.emplace_back(waitGraph(padd.name), static_cast<float>(waitMs));
    }
    // A padd that ran nothing this second was idle: its graphs move on too
    for (const std::string &padd : padds_) {
      if (!ranThisSecond.contains(padd)) {
        values.emplace_back(busyGraph(padd), 0.0f);
        values.emplace_back(waitGraph(padd), 0.0f);
      }
    }
    addToGraphs(values);
  }

private:
  // Gives each padd seen for the first time this second its graphs, all
  // in one snapshot (not one per graph: at the start every padd is new)
  void addGraphsForNewPadds(const MessageStats::Snapshot &second) {
    ControlValueMap graphs;
    for (const MessageStats::PaddCounts &padd : second.padds) {
      if (!padds_.insert(padd.name).second)
        continue;
      graphs.emplace(
          busyGraph(padd.name),
          ControlType::Graph(kHistory, ControlType::Graph::Style::Bars, {{0.0f, 100.0f}}, "%"));
      graphs.emplace(
          waitGraph(padd.name),
          ControlType::Graph(kHistory, ControlType::Graph::Style::Line, std::nullopt, " ms"));
    }
    if (!graphs.empty())
      addControls(graphs);
  }

  std::optional<MessageStats::Reader> reader_;
  std::set<std::string> padds_; // those with graphs
};

} // namespace ScratchPadd
