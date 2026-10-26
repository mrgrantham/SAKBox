#pragma once

// Reads WAV files with integer arithmetic only, for where floating point is
// slow (the Nintendo DS, whose AudioPadd loads sound files with it).
// Desktops and the 3DS read sound files with miniaudio instead.
//
// It reads uncompressed PCM, 8- and 16-bit, any rate and channel count, and
// IMA-ADPCM (WAV format 0x11, 4 bits a sample: a quarter of 16-bit's size),
// which it decodes to 16-bit. It also writes them, for tests and for files
// made when a program is built.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ScratchPadd::Audio::Wav {

// Samples as read: 8-bit (signed, as the DS plays them) or 16-bit,
// interleaved by channel
struct Sound {
  std::uint32_t rate = 0;
  std::uint16_t channels = 0;
  std::uint16_t bits = 0; // 8 or 16
  std::vector<std::int8_t> samples8;
  std::vector<std::int16_t> samples16;

  std::size_t frames() const {
    return channels ? (bits == 8 ? samples8.size() : samples16.size()) / channels : 0;
  }
};

// Reads a WAV file's bytes into `sound`, or returns false with the reason in
// `error`
bool read(const std::uint8_t *data, std::size_t size, Sound &sound, std::string &error);

// A WAV file's bytes: 16-bit PCM, 8-bit PCM, or IMA-ADPCM (mono only)
std::vector<std::uint8_t> write16(const std::vector<std::int16_t> &interleaved, std::uint32_t rate,
                                  std::uint16_t channels);
std::vector<std::uint8_t> write8(const std::vector<std::int8_t> &interleaved, std::uint32_t rate,
                                 std::uint16_t channels);
std::vector<std::uint8_t> writeImaAdpcm(const std::vector<std::int16_t> &mono, std::uint32_t rate);

} // namespace ScratchPadd::Audio::Wav
