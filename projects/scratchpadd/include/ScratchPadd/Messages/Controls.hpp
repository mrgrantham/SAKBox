#pragma once

// These data structures will handle the lifecycle of generic controls that are advertised by each padd.

namespace ScratchPadd {
struct BaseControlType {
  virtual void generateControlValueType(){};
};

namespace ControlType {
struct Double : public BaseControlType {
  double value{0.0};
  std::optional<std::pair<double, double>> enforcableRange;

  Double(double initialValue = 0.0,
         std::optional<std::pair<double, double>> range = std::nullopt) :
    value(initialValue),
    enforcableRange(range){
  }
};

struct Integer : public BaseControlType {
  int32_t value;
  std::optional<std::pair<int32_t, int32_t>> enforcableRange;
  Integer(int32_t initialValue = 0.0,
          std::optional<std::pair<int32_t, int32_t>> range = std::nullopt) {
    value = initialValue;
    enforcableRange = range;
  }
};

struct Boolean : public BaseControlType {
  bool value;
  Boolean(
      bool initialValue = 0.0) {
    value = initialValue;
  }
};

struct String : public BaseControlType {
  std::string value;
  std::optional<std::vector<std::string>> enforcableRange;
  std::optional<std::function<void(std::string)>> handler;
  String(std::string initialValue,
         std::optional<std::vector<std::string>> range = std::nullopt
         ) {
    value = initialValue;
    enforcableRange = range;
  }
};

} // namespace ControlType
#define ControlTypes                                                           \
  ScratchPadd::ControlType::Double, ScratchPadd::ControlType::Integer,         \
      ScratchPadd::ControlType::Boolean, ScratchPadd::ControlType::String
using ControlTypeVariant = std::variant<ControlTypes>;
using ControlValueMap = std::unordered_map<std::string, ControlTypeVariant>;
using ControlRefMap = std::unordered_map<std::string, ControlTypeVariant>;

namespace MessageType {


struct ControlSnapshot {
  std::string paddName;
  ControlValueMap controlMap;
};

// Send back to all Padds when there is a change in a value on a control panel
struct ControlChange {
  std::string paddName;
  std::string controlName;
  ControlTypeVariant controlTypeValue;
};

// initial request to get all panels with available controls
struct ControlRequest {
  std::optional<std::string> paddName;
};

}
}