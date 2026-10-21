#pragma once

#include <string>

namespace ScratchPadd {

class Base;
template <typename... Workers> class SystemImplementation;
struct PaddTestAccess;

// Names one padd, to send it messages (Base::sendTo) or to know which padd
// a message came from (a handler's `from`). It can be copied, compared and
// stored, but not used to reach the padd itself: padds only talk to each
// other through messages, and an address carries no way to call one.
// Get one from Base::address(), Base::addressOf<Padd>() or
// Base::addressOf("Name"). An empty address (no padd) is false.
class PaddAddress {
public:
  PaddAddress() = default;

  // Whether this names a padd
  explicit operator bool() const { return padd_ != nullptr; }

  bool operator==(const PaddAddress &) const = default;

  // The padd's name (Base::paddName()), or "(no padd)" for an empty address
  const std::string &name() const;

private:
  explicit PaddAddress(Base *padd) : padd_(padd) {}

  Base *padd_ = nullptr;

  // Only the padds' base class and the system can open an address
  friend class Base;
  template <typename... Workers> friend class SystemImplementation;
  friend struct PaddTestAccess;
};

} // namespace ScratchPadd
