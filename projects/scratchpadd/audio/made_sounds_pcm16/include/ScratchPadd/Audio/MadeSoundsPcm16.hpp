#pragma once

// The sounds made in code (MadeSounds.hpp), rendered when the program is
// built: 16-bit mono samples at kRate, compiled in. For where making them in
// float when the program runs is slow: on the Nintendo DS, which has no
// floating-point hardware, making the chime, knock and blast took about 24 s.
// Load them with AudioPadd's LoadSoundPcm16. (They're the same sounds:
// MadeSoundsPcm16_test checks.)

#include <cstdint>
#include <vector>

namespace ScratchPadd::Audio::MadeSoundsPcm16 {

inline constexpr std::uint32_t kRate = 16384;

std::vector<std::int16_t> chime();
std::vector<std::int16_t> knock();
std::vector<std::int16_t> blast();
std::vector<std::int16_t> note(); // MadeSounds::note(): C5, for an instrument

} // namespace ScratchPadd::Audio::MadeSoundsPcm16
