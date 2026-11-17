// ActivityPadd.hpp - what the app is doing, as graphs on a tab: every second,
// the messages sent that second, and for each padd how busy it was (its
// share of the second spent running jobs: message handlers, repeat() and
// pushed work) and the longest a job waited for it. Each graph keeps the
// last minute. A padd's graphs appear the first second it runs a job. With
// the counting allocator linked (MemoryStats.hpp), allocations a second too,
// in all and each padd's; and the heap in use, where the platform says.
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
#include <ScratchPadd/MemoryStats.hpp>
#include <ScratchPadd/MessageStats.hpp>

#include <chrono>
#include <cstdint>
#include <map>
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
  // (with the counting allocator linked: MemoryStats.hpp)
  static constexpr const char *kAllocations = "Allocations a second";
  // (where the platform says: MemoryStats::heap())
  static constexpr const char *kHeap = "Heap in use";

  explicit ActivityPadd(System *system) : Base(system) {}
  std::string name() override { return "Activity"; }
  Runs runs() override { return Runs::WhenIdle; }
  std::optional<std::chrono::duration<double>> repeatInterval() override { return kEvery; }
  ControlValueMap generateControls() override {
    ControlValueMap graphs{
        {kMessages, ControlType::Graph(kHistory, ControlType::Graph::Style::Line)}};
    if (MemoryStats::linked())
      graphs.emplace(kAllocations, ControlType::Graph(kHistory, ControlType::Graph::Style::Line));
    if (MemoryStats::heap().known)
      graphs.emplace(kHeap, ControlType::Graph(kHistory, ControlType::Graph::Style::Line,
                                               std::nullopt, " KB"));
    return graphs;
  }
  void starting() override {
    reader_.emplace(system_->messageStats());
    if (MemoryStats::linked())
      memory_.emplace();
  }
  void finishing() override {
    reader_.reset();
    memory_.reset();
  }

  // The names of a padd's graphs
  static std::string busyGraph(const std::string &padd) { return padd + " busy"; }
  static std::string waitGraph(const std::string &padd) { return padd + " longest wait"; }
  static std::string allocationsGraph(const std::string &padd) { return padd + " allocations"; }

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

    // Memory: allocations a second (in all, and each padd's below), and the
    // heap in use, as the platform says
    std::map<std::string, std::uint64_t> allocations;
    if (memory_) {
      const MemoryStats::Snapshot memory = memory_->take();
      values.emplace_back(kAllocations, static_cast<float>(memory.all.allocations / seconds));
      for (const MemoryStats::PaddCounts &padd : memory.padds)
        allocations[padd.name] = padd.counts.allocations;
    }
    if (const MemoryStats::Heap heap = MemoryStats::heap(); heap.known)
      values.emplace_back(kHeap, static_cast<float>(static_cast<double>(heap.inUse) / 1024));

    addGraphsForNewPadds(second);
    std::set<std::string> ranThisSecond;
    for (const MessageStats::PaddCounts &padd : second.padds) {
      ranThisSecond.insert(padd.name);
      const double busy = std::chrono::duration<double>(padd.busy()).count() / seconds;
      const double waitMs = std::chrono::duration<double, std::milli>(padd.longestWait).count();
      values.emplace_back(busyGraph(padd.name), static_cast<float>(100.0 * busy));
      values.emplace_back(waitGraph(padd.name), static_cast<float>(waitMs));
      if (memory_)
        values.emplace_back(
            allocationsGraph(padd.name),
            static_cast<float>(static_cast<double>(allocations[padd.name]) / seconds));
    }
    // A padd that ran nothing this second was idle: its graphs move on too
    for (const std::string &padd : padds_) {
      if (!ranThisSecond.contains(padd)) {
        values.emplace_back(busyGraph(padd), 0.0f);
        values.emplace_back(waitGraph(padd), 0.0f);
        if (memory_)
          values.emplace_back(allocationsGraph(padd), 0.0f);
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
      if (memory_)
        graphs.emplace(allocationsGraph(padd.name),
                       ControlType::Graph(kHistory, ControlType::Graph::Style::Line));
    }
    if (!graphs.empty())
      addControls(graphs);
  }

  std::optional<MessageStats::Reader> reader_;
  std::optional<MemoryStats::Reader> memory_; // (with the counting allocator linked)
  std::set<std::string> padds_;               // those with graphs
};

} // namespace ScratchPadd
