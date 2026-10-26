#pragma once

// Reports every `new` in the program to the audio core, which counts those
// made while mixing (see ScratchPadd/Audio/RealTime.hpp): include it in one
// source file of a test that checks mixing allocates nothing.

#include <ScratchPadd/Audio/RealTime.hpp>

#include <cstdlib>
#include <new>

void *operator new(std::size_t size) {
  ScratchPadd::Audio::noteAllocation();
  if (void *memory = std::malloc(size == 0 ? 1 : size)) {
    return memory;
  }
  throw std::bad_alloc();
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
