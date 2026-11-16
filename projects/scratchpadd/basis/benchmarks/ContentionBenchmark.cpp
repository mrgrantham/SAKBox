// padd_contention_benchmark: what having many padds costs an app, measured
// on this device, so "how many padds is too many?" has an answer in numbers.
//
// 1. Shared work: the same total work split among 1 to 32 padds. While
//    there are cores to spare, more padds finish sooner; past that they
//    take turns, and the extra time is what the switching costs.
// 2. Timing under load: a padd that repeats every 2 ms (as a renderer or
//    a mixer does), with 0 to 32 other padds busy all the time. How late its
//    repeat() comes is what the busy padds cost it: at the default
//    priority (Runs::Steadily), and at Runs::Urgently.
// 3. Idle padds: what each one costs to start and in memory, and that idle
//    padds use no CPU (they sleep until work comes).
//
//   bazel run -c opt //projects/scratchpadd/basis:padd_contention_benchmark
#include <ScratchPadd/ScratchPadd.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#elif defined(_WIN32)
#ifndef NOMINMAX // (the toolchain may define it already)
#define NOMINMAX
#endif
#include <psapi.h>
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>

#include <fstream>
#endif

namespace {

using Clock = std::chrono::steady_clock;
using Runs = ScratchPadd::Runs;

double millisecondsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// --- The work: a fixed amount of arithmetic the compiler can't skip

std::atomic<std::uint32_t> workSink{0};

void doWork(int iterations) {
  std::uint32_t x = 2463534242u;
  for (int i = 0; i < iterations; ++i) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
  }
  workSink.fetch_add(x, std::memory_order_relaxed);
}

// How many iterations of doWork() take `microseconds` on this machine
int iterationsFor(int microseconds) {
  constexpr int kTrial = 1'000'000;
  const auto start = Clock::now();
  doWork(kTrial);
  const double perMicrosecond = kTrial / (millisecondsSince(start) * 1000.0);
  return std::max(1, static_cast<int>(perMicrosecond * microseconds));
}

// --- What the process uses, for part 3

// Memory the process has resident, in bytes
std::size_t residentBytes() {
#if defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count);
  return info.resident_size;
#elif defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters);
  return counters.WorkingSetSize;
#else
  std::ifstream statm("/proc/self/statm");
  std::size_t pages = 0, resident = 0;
  statm >> pages >> resident;
  return resident * static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

// CPU time the process has used, every thread, in milliseconds
double cpuMilliseconds() {
#if defined(_WIN32)
  FILETIME created, exited, kernel, user;
  GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
  const auto ticks = [](FILETIME t) {
    return (static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
  };
  return static_cast<double>(ticks(kernel) + ticks(user)) / 10'000.0; // (100 ns ticks)
#else
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  const auto ms = [](timeval t) { return t.tv_sec * 1000.0 + t.tv_usec / 1000.0; };
  return ms(usage.ru_utime) + ms(usage.ru_stime);
#endif
}

// --- The padds

struct Job {
  int iterations = 0;
};
struct StayBusy {
  int iterations = 0; // a chunk: between chunks, the padd's other work gets in
};

// Shared by the workers of one run
std::atomic<int> jobsLeft{0};
std::promise<void> *allJobsDone = nullptr;
std::atomic<bool> stayingBusy{false};

// Does the work it's sent. Many at once, each its own type (Worker<0>,
// Worker<1>, ...), as a system's padds are.
template <int Id> class Worker : public ScratchPadd::Base {
public:
  explicit Worker(ScratchPadd::System *system) : Base(system) {
    receive<Job>([](const Job &job) {
      doWork(job.iterations);
      if (jobsLeft.fetch_sub(1) == 1)
        allJobsDone->set_value();
    });
    receive<StayBusy>([this](const StayBusy &busy) { keepBusy(busy.iterations); });
  }
  std::string name() override { return "Worker" + std::to_string(Id); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

private:
  // A chunk of work, then the next one queued behind whatever else came
  void keepBusy(int iterations) {
    doWork(iterations);
    if (stayingBusy)
      push([this, iterations] { keepBusy(iterations); });
  }
};

// Repeats every 2 ms and notes when each repeat() came
template <Runs Priority> class Probe : public ScratchPadd::Base {
public:
  static constexpr std::chrono::milliseconds kInterval{2};

  explicit Probe(ScratchPadd::System *system) : Base(system) { times_.reserve(4096); }
  std::string name() override { return "Probe"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  Runs runs() override { return Priority; }
  std::optional<std::chrono::duration<double>> repeatInterval() override { return kInterval; }
  void repeat() override {
    if (recording_)
      times_.push_back(Clock::now());
  }

  void record(bool on) { recording_ = on; }
  // How much later than kInterval after the one before each repeat() came, in µs
  std::vector<double> lateness() const {
    std::vector<double> late;
    late.reserve(times_.size());
    for (std::size_t i = 1; i < times_.size(); ++i) {
      const double gap =
          std::chrono::duration<double, std::micro>(times_[i] - times_[i - 1]).count();
      late.push_back(std::max(0.0, gap - 1000.0 * kInterval.count()));
    }
    return late;
  }

private:
  std::atomic<bool> recording_{false};
  std::vector<Clock::time_point> times_; // (the probe's thread, until it's stopped)
};

// Sends from a padd's own thread, as an app's padds do
class Driver : public ScratchPadd::Base {
public:
  explicit Driver(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Driver"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  // Runs `job` on the driver's thread and waits for it
  template <typename Job> void run(Job job) {
    std::promise<void> ran;
    push([&] {
      job(*this);
      ran.set_value();
    });
    ran.get_future().wait();
  }
  using Base::addressOf;
  using Base::send;
  using Base::sendTo;
};

// A system of the given padds, running until it goes out of scope
template <typename... Padds> struct Running {
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread thread;
  Running() {
    system.instantiate();
    thread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~Running() { stop(); }
  // Stops every padd (the busy ones first) and waits for them
  void stop() {
    if (!thread.joinable())
      return;
    stayingBusy = false;
    system.stop();
    thread.join();
  }
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
};

// Calls body(running) with a system of `Extra` padds and Worker<0> to
// Worker<Count - 1>
template <int Count, typename... Extra, typename Body> void withWorkers(Body body) {
  [&]<int... Ids>(std::integer_sequence<int, Ids...>) {
    Running<Extra..., Worker<Ids>...> running;
    body(running);
  }(std::make_integer_sequence<int, Count>{});
}

double percentile(std::vector<double> values, double fraction) {
  if (values.empty())
    return 0;
  std::sort(values.begin(), values.end());
  return values[static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1))];
}

// --- 1. Shared work

constexpr int kJobs = 2000;
constexpr int kJobMicroseconds = 100;

// Milliseconds for kJobs jobs, dealt out evenly to Count workers
template <int Count> double sharedWork(int iterations) {
  double best = 0;
  withWorkers<Count, Driver>([&](auto &running) {
    for (int run = 0; run < 3; ++run) { // (the best of 3)
      std::promise<void> done;
      allJobsDone = &done;
      jobsLeft = kJobs;
      const auto start = Clock::now();
      running.template padd<Driver>().run([&](Driver &driver) {
        std::vector<ScratchPadd::PaddAddress> workers;
        workers.reserve(Count);
        for (int w = 0; w < Count; ++w)
          workers.push_back(driver.addressOf("Worker" + std::to_string(w)));
        for (int job = 0; job < kJobs; ++job)
          driver.sendTo(workers[static_cast<std::size_t>(job % Count)], Job{iterations});
      });
      done.get_future().wait();
      const double ms = millisecondsSince(start);
      best = run == 0 ? ms : std::min(best, ms);
    }
  });
  return best;
}

template <int Count> void sharedWorkRow(int iterations, double oneWorker, unsigned cores) {
  const double ms = sharedWork<Count>(iterations);
  const double speedUp = oneWorker / ms;
  const double possible = std::min<double>(Count, cores);
  std::printf("  %5d  %8.1f ms  %6.2fx  %5.0f%% of %2.0f cores' worth\n", Count, ms, speedUp,
              100.0 * speedUp / possible, possible);
}

// --- 2. Timing under load

constexpr std::chrono::milliseconds kRecordFor{1000};
constexpr int kBusyChunkMicroseconds = 50;

// How late the probe's repeat() came, with Count padds busy
template <Runs Priority, int Count> std::vector<double> probeLateness(int chunkIterations) {
  std::vector<double> late;
  withWorkers<Count, Driver, Probe<Priority>>([&](auto &running) {
    stayingBusy = true;
    if constexpr (Count > 0) {
      running.template padd<Driver>().run(
          [&](Driver &driver) { driver.send(StayBusy{chunkIterations}); });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // (settled)
    auto &probe = running.template padd<Probe<Priority>>();
    probe.record(true);
    std::this_thread::sleep_for(kRecordFor);
    probe.record(false);
    running.stop(); // (the probe's thread done, its times can be read)
    late = probe.lateness();
  });
  return late;
}

template <int Count> void latenessRow(int chunkIterations) {
  const std::vector<double> steady = probeLateness<Runs::Steadily, Count>(chunkIterations);
  const std::vector<double> urgent = probeLateness<Runs::Urgently, Count>(chunkIterations);
  std::printf("  %5d   %7.0f %7.0f %8.0f    %7.0f %7.0f %8.0f\n", Count, percentile(steady, 0.5),
              percentile(steady, 0.99), percentile(steady, 1.0), percentile(urgent, 0.5),
              percentile(urgent, 0.99), percentile(urgent, 1.0));
}

// --- 3. Idle padds

template <int Count> void idleRow(std::size_t baseline) {
  const auto start = Clock::now();
  withWorkers<Count, Driver>([&](auto &) {
    const double startMs = millisecondsSince(start);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const double resident = static_cast<double>(residentBytes()) - static_cast<double>(baseline);
    const double cpuBefore = cpuMilliseconds();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const double idleCpu = cpuMilliseconds() - cpuBefore;
    std::printf("  %5d  %8.2f ms  %7.1f µs  %8.0f KB  %6.1f KB   %6.2f ms\n", Count, startMs,
                1000.0 * startMs / Count, resident / 1024.0, resident / 1024.0 / Count, idleCpu);
  });
}

} // namespace

int main() {
  ScratchPadd::Logger::setTerminalLevel(spdlog::level::warn);
  const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
  std::printf("Padd contention on this machine (%u cores)\n", cores);

  const int jobIterations = iterationsFor(kJobMicroseconds);
  std::printf("\n1. The same work (%d jobs of %d µs) shared among N padds, the best of 3:\n\n",
              kJobs, kJobMicroseconds);
  std::printf("  padds      time    speed-up  of what the cores allow\n");
  const double oneWorker = sharedWork<1>(jobIterations);
  std::printf("  %5d  %8.1f ms  %6.2fx\n", 1, oneWorker, 1.0);
  sharedWorkRow<2>(jobIterations, oneWorker, cores);
  sharedWorkRow<4>(jobIterations, oneWorker, cores);
  sharedWorkRow<8>(jobIterations, oneWorker, cores);
  sharedWorkRow<16>(jobIterations, oneWorker, cores);
  sharedWorkRow<32>(jobIterations, oneWorker, cores);

  const int chunkIterations = iterationsFor(kBusyChunkMicroseconds);
  std::printf("\n2. A padd repeating every 2 ms, while N padds are busy (%d µs chunks), for 1 s:\n"
              "   how late each repeat() came, in µs\n\n",
              kBusyChunkMicroseconds);
  std::printf("  busy     Steadily (default)          Urgently\n");
  std::printf("  padds    median     99%%      max     median     99%%      max\n");
  latenessRow<0>(chunkIterations);
  latenessRow<1>(chunkIterations);
  latenessRow<4>(chunkIterations);
  latenessRow<8>(chunkIterations);
  latenessRow<16>(chunkIterations);
  latenessRow<32>(chunkIterations);

  std::printf("\n3. N idle padds: starting them, memory, and CPU used in 0.5 s idle:\n\n");
  std::printf("  padds     start   per padd    resident  per padd   idle CPU\n");
  const std::size_t baseline = residentBytes();
  idleRow<1>(baseline);
  idleRow<8>(baseline);
  idleRow<32>(baseline);
  idleRow<64>(baseline);
  return 0;
}
