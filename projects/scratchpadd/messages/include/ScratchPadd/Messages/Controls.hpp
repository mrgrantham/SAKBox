#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// These data structures will handle the lifecycle of generic controls that are
// advertised by each padd.

namespace ScratchPadd {
// The control types are plain values, held in a ControlTypeVariant. (They
// used to share a base class with one empty virtual function, which nothing
// called; Clang warns when such a class is destroyed through the variant.)
namespace ControlType {
struct Double {
  double value{0.0};
  std::optional<std::pair<double, double>> enforcableRange;

  Double(double initialValue = 0.0, std::optional<std::pair<double, double>> range = std::nullopt)
      : value(initialValue), enforcableRange(std::move(range)) {}
};

struct Integer {
  int32_t value;
  std::optional<std::pair<int32_t, int32_t>> enforcableRange;
  Integer(int32_t initialValue = 0, std::optional<std::pair<int32_t, int32_t>> range = std::nullopt)
      : value(initialValue), enforcableRange(std::move(range)) {}
};

struct Boolean {
  bool value;
  Boolean(bool initialValue = false) : value(initialValue) {}
};

struct String {
  std::string value;
  std::optional<std::vector<std::string>> enforcableRange;
  std::optional<std::function<void(std::string)>> handler;
  String(std::string initialValue, std::optional<std::vector<std::string>> range = std::nullopt)
      : value(std::move(initialValue)), enforcableRange(std::move(range)) {}
};

} // namespace ControlType
#define ControlTypes                                                                               \
  ScratchPadd::ControlType::Double, ScratchPadd::ControlType::Integer,                             \
      ScratchPadd::ControlType::Boolean, ScratchPadd::ControlType::String
using ControlTypeVariant = std::variant<ControlTypes>;
using ControlValueMap = std::unordered_map<std::string, ControlTypeVariant>;
using ControlRefMap = std::unordered_map<std::string, ControlTypeVariant>;

namespace MessageType {

struct ControlSnapshot {
  std::string paddName;
  ControlValueMap controlMap;
};

// An edit made on a control panel, sent straight to the padd whose control
// it is (Base::sendTo, to the address its snapshot came from). The padd
// applies it and replies to every panel with a fresh snapshot.
struct ControlChange {
  std::string controlName;
  ControlTypeVariant controlTypeValue;
};

// Asks for controls: every padd with controls replies to the sender with a
// ControlSnapshot. Broadcast it to ask them all, or send it to one padd
// (Base::sendTo) to ask just that one.
struct ControlRequest {};

} // namespace MessageType
} // namespace ScratchPadd