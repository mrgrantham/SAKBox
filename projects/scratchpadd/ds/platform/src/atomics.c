// std::atomic on the Nintendo DS.
//
// The DS's CPU (an ARM9, ARMv5) has no atomic read-modify-write
// instructions, so GCC turns std::atomic operations into calls to
// libatomic's functions (__atomic_fetch_add_4 and the like), and devkitARM
// has no libatomic for the DS. These are those functions. The DS has one
// core, and calico only switches threads on an interrupt, so each operation
// is made atomic by switching interrupts off for its few instructions.
//
// Each is defined under a name of its own and given libatomic's name with an
// asm label, as GCC won't let C define a function named like its builtins.

#include <calico/arm/common.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define LOCKED(statements)                                                                         \
  do {                                                                                             \
    const ArmIrqState state = armIrqLockByPsr();                                                   \
    statements;                                                                                    \
    armIrqUnlockByPsr(state);                                                                      \
  } while (0)

// Each operation for one size: SIZE bytes, held in TYPE
#define ATOMICS(SIZE, TYPE)                                                                        \
  TYPE ds_atomic_load_##SIZE(const volatile void *p, int order) __asm__("__atomic_load_" #SIZE);   \
  TYPE ds_atomic_load_##SIZE(const volatile void *p, int order) {                                  \
    (void)order;                                                                                   \
    TYPE value;                                                                                    \
    LOCKED(value = *(const volatile TYPE *)p);                                                     \
    return value;                                                                                  \
  }                                                                                                \
  void ds_atomic_store_##SIZE(volatile void *p, TYPE value,                                        \
                              int order) __asm__("__atomic_store_" #SIZE);                         \
  void ds_atomic_store_##SIZE(volatile void *p, TYPE value, int order) {                           \
    (void)order;                                                                                   \
    LOCKED(*(volatile TYPE *)p = value);                                                           \
  }                                                                                                \
  TYPE ds_atomic_exchange_##SIZE(volatile void *p, TYPE value,                                     \
                                 int order) __asm__("__atomic_exchange_" #SIZE);                   \
  TYPE ds_atomic_exchange_##SIZE(volatile void *p, TYPE value, int order) {                        \
    (void)order;                                                                                   \
    TYPE old;                                                                                      \
    LOCKED(old = *(volatile TYPE *)p; *(volatile TYPE *)p = value);                                \
    return old;                                                                                    \
  }                                                                                                \
  bool ds_atomic_compare_exchange_##SIZE(volatile void *p, void *expected, TYPE desired,           \
                                         int success,                                              \
                                         int failure) __asm__("__atomic_compare_exchange_" #SIZE); \
  bool ds_atomic_compare_exchange_##SIZE(volatile void *p, void *expected, TYPE desired,           \
                                         int success, int failure) {                               \
    (void)success;                                                                                 \
    (void)failure;                                                                                 \
    bool exchanged;                                                                                \
    LOCKED(                                                                                        \
        const TYPE current = *(volatile TYPE *)p; exchanged = current == *(TYPE *)expected;        \
        if (exchanged) { *(volatile TYPE *)p = desired; } else { *(TYPE *)expected = current; });  \
    return exchanged;                                                                              \
  }                                                                                                \
  FETCH_OPS(SIZE, TYPE, add, +)                                                                    \
  FETCH_OPS(SIZE, TYPE, sub, -)                                                                    \
  FETCH_OPS(SIZE, TYPE, and, &)                                                                    \
  FETCH_OPS(SIZE, TYPE, or, |)                                                                     \
  FETCH_OPS(SIZE, TYPE, xor, ^)

// __atomic_fetch_NAME_SIZE (returns the old value) and
// __atomic_NAME_fetch_SIZE (the new one)
#define FETCH_OPS(SIZE, TYPE, NAME, OP)                                                            \
  TYPE ds_atomic_fetch_##NAME##_##SIZE(volatile void *p, TYPE value,                               \
                                       int order) __asm__("__atomic_fetch_" #NAME "_" #SIZE);      \
  TYPE ds_atomic_fetch_##NAME##_##SIZE(volatile void *p, TYPE value, int order) {                  \
    (void)order;                                                                                   \
    TYPE old;                                                                                      \
    LOCKED(old = *(volatile TYPE *)p; *(volatile TYPE *)p = (TYPE)(old OP value));                 \
    return old;                                                                                    \
  }                                                                                                \
  TYPE ds_atomic_##NAME##_fetch_##SIZE(volatile void *p, TYPE value,                               \
                                       int order) __asm__("__atomic_" #NAME "_fetch_" #SIZE);      \
  TYPE ds_atomic_##NAME##_fetch_##SIZE(volatile void *p, TYPE value, int order) {                  \
    return (TYPE)(ds_atomic_fetch_##NAME##_##SIZE(p, value, order) OP value);                      \
  }

ATOMICS(1, uint8_t)
ATOMICS(2, uint16_t)
ATOMICS(4, uint32_t)
ATOMICS(8, uint64_t)
