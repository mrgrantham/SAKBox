// SharedState.hpp - shared state: the latest value of a type, published by
// one padd and read by any, whenever it likes. Nothing queues and no one is
// woken, unlike a message: a message is for something that happens and
// mustn't be lost, shared state for a current value (the controller now,
// the world as the physics last left it), read when it's needed.
//
//   publishState(Input::read());                      // in the publisher
//   const auto pad = currentState<Input::Gamepad>();  // in any reader
//   if (pad.version == 0) { /* nothing published yet */ }
//
// Base's publishState() and currentState() find a slot here by the value's
// type, and a name when an app has several of one type. The system owns the
// slots, so they outlive its padds.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

namespace ScratchPadd {

class Base;

// A copy of a shared value, and its version: 0 if it was never published,
// then one more each time it is
template <typename T> struct State {
  T value{};
  std::uint64_t version = 0;
};

// What may be shared: a plain struct, copied whole each publish and read
template <typename T>
concept Shareable = std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>;

class SharedStateSlotBase {
public:
  virtual ~SharedStateSlotBase() = default;

  // The padd that publishes here: the first to, and only it (the rest are
  // refused). Returns whether `padd` may.
  bool claimFor(const Base *padd) {
    std::lock_guard lock(mutex_);
    if (!publisher_)
      publisher_ = padd;
    return publisher_ == padd;
  }

protected:
  // Held only while a value is copied in or out, so a publisher waits at
  // most for one reader's copy (and readers for each other's)
  std::mutex mutex_;
  const Base *publisher_ = nullptr;
};

template <Shareable T> class SharedStateSlot : public SharedStateSlotBase {
public:
  void publish(const T &value) {
    std::lock_guard lock(mutex_);
    state_.value = value;
    ++state_.version;
  }
  State<T> latest() {
    std::lock_guard lock(mutex_);
    return state_;
  }

private:
  State<T> state_;
};

// Every slot of a system, by type and name, made by whichever padd asks
// first (to publish or read)
class SharedStates {
public:
  template <Shareable T> SharedStateSlot<T> &slot(std::string_view name) {
    std::lock_guard lock(mutex_);
    auto &found = slots_[Key{std::type_index(typeid(T)), std::string(name)}];
    if (!found)
      found = std::make_unique<SharedStateSlot<T>>();
    return static_cast<SharedStateSlot<T> &>(*found);
  }

  struct Key {
    std::type_index type;
    std::string name;
    bool operator==(const Key &) const = default;
  };
  struct KeyHash {
    std::size_t operator()(const Key &key) const {
      return key.type.hash_code() ^ (std::hash<std::string>{}(key.name) * 31);
    }
  };

private:
  std::mutex mutex_;
  std::unordered_map<Key, std::unique_ptr<SharedStateSlotBase>, KeyHash> slots_;
};

} // namespace ScratchPadd
