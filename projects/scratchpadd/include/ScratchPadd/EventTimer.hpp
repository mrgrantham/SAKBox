#pragma once
#include <thread>
namespace ScratchPadd {
class EventTimer {
  bool is_active_{false};
  bool should_stop_{false};
  std::thread timer_thread_;
  public:
  template <typename Event>
  void startRepeatingEvent(const Event &event, int interval_between_events) {
    timer_thread_ = std::thread([=,this]{
      while (should_stop_ == false) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_between_events));
        if (should_stop_) break;
        event();
      };
      is_active_ = false;
    });
    timer_thread_.detach();
  }

  template <typename Event>
  void startTimedEvent(const Event &event, int delay_until_event) {
    timer_thread_ = std::thread([=,this]{
      if (should_stop_) return;
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_until_event));
      if (should_stop_) return;
      event();
    });
    timer_thread_.detach();
  }

  void stop() {
    should_stop_ = true;
    if (timer_thread_.joinable()) {
      timer_thread_.join();
    }
 
    is_active_ = false;
  }

  bool isActive() {
    return is_active_;
  }
};
}