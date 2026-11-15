// message_benchmark: what a ScratchPadd message costs, measured, so pooling
// messages or streaming them can be decided from numbers (with
// MessageStats, which shows what an app actually sends):
//
// - round trip: a padd sends another a message and waits for the answer
//   before sending the next (latency, through two queues and two wakes);
// - throughput: a burst of messages to one padd, for payloads of 8 bytes
//   to 4 KB, and for payloads that allocate (a string, a vector), to see
//   what copying and allocating each message costs;
// - broadcast: one message reaching 1, 4 or 8 padds.
//
// Each case runs 3 times; the median is shown.
//
//   bazel run -c opt //projects/scratchpadd/basis:message_benchmark
#include <ScratchPadd/ScratchPadd.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Ping {
  int n = 0;
};
struct Pong {
  int n = 0;
};
template <std::size_t Size> struct Payload {
  std::array<char, Size> bytes{};
};
struct Text {
  std::string text;
};
struct Floats {
  std::vector<float> values;
};

// Counts what it receives; says when it has had `expect`
class CounterPadd : public ScratchPadd::Base {
public:
  explicit CounterPadd(ScratchPadd::System *system, int id = 0) : Base(system), id_(id) {
    receive<Payload<8>>([this](const Payload<8> &) { count(); });
    receive<Payload<64>>([this](const Payload<64> &) { count(); });
    receive<Payload<512>>([this](const Payload<512> &) { count(); });
    receive<Payload<4096>>([this](const Payload<4096> &) { count(); });
    receive<Text>([this](const Text &) { count(); });
    receive<Floats>([this](const Floats &) { count(); });
    receive<Ping>(
        [this](const Ping &ping, ScratchPadd::PaddAddress from) { reply(from, Pong{ping.n}); });
  }
  std::string name() override { return "Counter" + std::to_string(id_); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void expect(int count, std::promise<void> *done) {
    received_ = 0;
    expect_ = count;
    done_ = done;
  }

private:
  void count() {
    if (++received_ == expect_ && done_)
      done_->set_value();
  }
  int id_;
  int received_ = 0, expect_ = 0;
  std::promise<void> *done_ = nullptr;
};
template <int Id> class Counter : public CounterPadd {
public:
  explicit Counter(ScratchPadd::System *system) : CounterPadd(system, Id) {}
};

// Sends; runs jobs on its own thread, as a padd's code does
class SenderPadd : public ScratchPadd::Base {
public:
  explicit SenderPadd(ScratchPadd::System *system) : Base(system) {
    receive<Pong>([this](const Pong &pong) {
      if (pong.n >= roundTrips_) {
        done_->set_value();
        return;
      }
      sendTo(counter_, Ping{pong.n + 1});
    });
  }
  std::string name() override { return "Sender"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  template <typename Job> void run(Job job) {
    std::promise<void> ran;
    push([&] {
      job(*this);
      ran.set_value();
    });
    ran.get_future().wait();
  }
  void roundTrips(int count, std::promise<void> *done) {
    roundTrips_ = count;
    done_ = done;
    counter_ = addressOf("Counter0");
    sendTo(counter_, Ping{1});
  }
  using Base::addressOf;
  using Base::send;
  using Base::sendTo;

private:
  int roundTrips_ = 0;
  std::promise<void> *done_ = nullptr;
  ScratchPadd::PaddAddress counter_;
};

template <typename... Padds> struct Running {
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread thread;
  Running() {
    system.instantiate();
    thread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~Running() {
    system.stop();
    thread.join();
  }
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
};

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

// Nanoseconds a message: a burst of `count` of `make()` sent to Counter0
template <typename Make> double burst(int count, Make make) {
  Running<SenderPadd, Counter<0>> running;
  auto &sender = running.padd<SenderPadd>();
  auto &counter = running.padd<Counter<0>>();
  std::vector<double> runs;
  for (int r = 0; r < 3; ++r) {
    std::promise<void> done;
    counter.expect(count, &done);
    const auto start = Clock::now();
    sender.run([&](SenderPadd &s) {
      const ScratchPadd::PaddAddress to = s.addressOf("Counter0");
      for (int i = 0; i < count; ++i)
        s.sendTo(to, make());
    });
    done.get_future().wait();
    runs.push_back(std::chrono::duration<double, std::nano>(Clock::now() - start).count() / count);
  }
  return median(runs);
}

// Nanoseconds a delivery: `count` broadcasts reaching every counter
template <typename... Counters> double broadcast(int count) {
  Running<SenderPadd, Counters...> running;
  std::vector<double> runs;
  for (int r = 0; r < 3; ++r) {
    std::vector<std::promise<void>> done(sizeof...(Counters));
    std::size_t i = 0;
    ((running.template padd<Counters>().expect(count, &done[i++])), ...);
    const auto start = Clock::now();
    running.template padd<SenderPadd>().run([&](SenderPadd &s) {
      for (int n = 0; n < count; ++n)
        s.send(Payload<64>{});
    });
    for (auto &d : done)
      d.get_future().wait();
    runs.push_back(std::chrono::duration<double, std::nano>(Clock::now() - start).count() /
                   (static_cast<double>(count) * sizeof...(Counters)));
  }
  return median(runs);
}

double roundTrip(int count) {
  Running<SenderPadd, Counter<0>> running;
  std::vector<double> runs;
  for (int r = 0; r < 3; ++r) {
    std::promise<void> done;
    const auto start = Clock::now();
    running.padd<SenderPadd>().run([&](SenderPadd &s) { s.roundTrips(count, &done); });
    done.get_future().wait();
    runs.push_back(std::chrono::duration<double, std::nano>(Clock::now() - start).count() / count);
  }
  return median(runs);
}

void line(const char *what, double nanoseconds) {
  std::printf("  %-44s %10.0f ns  %12.0f a second\n", what, nanoseconds, 1e9 / nanoseconds);
}

} // namespace

int main() {
  ScratchPadd::Logger::setTerminalLevel(spdlog::level::warn);
  std::printf("Message costs on this machine (median of 3 runs)\n\n");
  std::printf("Round trip (send, answer, next):\n");
  line("Ping to a padd and its Pong back", roundTrip(20000));

  std::printf("\nA burst to one padd, a message each:\n");
  line("8 bytes", burst(200000, [] { return Payload<8>{}; }));
  line("64 bytes", burst(200000, [] { return Payload<64>{}; }));
  line("512 bytes", burst(100000, [] { return Payload<512>{}; }));
  line("4 KB", burst(20000, [] { return Payload<4096>{}; }));
  line("a 64-character string (allocates)",
       burst(200000, [] { return Text{std::string(64, 'x')}; }));
  line("a vector of 64 floats (allocates)",
       burst(200000, [] { return Floats{std::vector<float>(64)}; }));

  std::printf("\nBroadcast (64 bytes), a delivery each:\n");
  line("to 1 padd", broadcast<Counter<0>>(100000));
  line("to 4 padds", broadcast<Counter<0>, Counter<1>, Counter<2>, Counter<3>>(50000));
  line("to 8 padds", broadcast<Counter<0>, Counter<1>, Counter<2>, Counter<3>, Counter<4>,
                               Counter<5>, Counter<6>, Counter<7>>(25000));
  return 0;
}
