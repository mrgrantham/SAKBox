// IntegerDivideNds.hpp - IntegerDivide.hpp's division on the Nintendo DS's
// hardware divider (IntegerDivide.hpp includes it in a DS build). The only
// code that uses the divider's registers: FixedPoint's divide does so
// through divide64.
//
// The registers are shared: a division is written to them and its result
// read back once they're no longer busy, and another thread starting one
// partway through would change the result. So each runs with interrupts
// locked (calico switches threads on interrupts), for the few dozen cycles
// it takes. libnds's own maths (divf32, sqrtf32, and the GL helpers that
// use them, such as gluLookAt) doesn't lock: code on another thread that
// calls it while these may run should hold a HardwareMathsLock around it
// (Cyclone's DS app does around gluLookAt).
#pragma once

#include <calico/arm/common.h>
#include <cstdint>
#include <nds/arm9/math.h>

namespace Flex::Math {

// Interrupts locked for as long as it exists: around a calculation on the
// divider or square root unit
class HardwareMathsLock {
public:
  HardwareMathsLock() : state_(armIrqLockByPsr()) {}
  ~HardwareMathsLock() { armIrqUnlockByPsr(state_); }
  HardwareMathsLock(const HardwareMathsLock &) = delete;
  HardwareMathsLock &operator=(const HardwareMathsLock &) = delete;

private:
  ArmIrqState state_;
};

inline Divided<std::int32_t> divide32(std::int32_t numerator, std::int32_t denominator) {
  HardwareMathsLock lock;
  while (REG_DIVCNT & DIV_BUSY) { // (one of libnds's, unlocked, may be running)
  }
  REG_DIVCNT = DIV_32_32;
  REG_DIV_NUMER_L = numerator;
  REG_DIV_DENOM_L = denominator;
  while (REG_DIVCNT & DIV_BUSY) {
  }
  return {REG_DIV_RESULT_L, REG_DIVREM_RESULT_L};
}

inline Divided<std::int64_t> divide64(std::int64_t numerator, std::int64_t denominator) {
  HardwareMathsLock lock;
  while (REG_DIVCNT & DIV_BUSY) {
  }
  REG_DIVCNT = DIV_64_64;
  REG_DIV_NUMER = numerator;
  REG_DIV_DENOM = denominator;
  while (REG_DIVCNT & DIV_BUSY) {
  }
  return {REG_DIV_RESULT, REG_DIVREM_RESULT};
}

} // namespace Flex::Math
