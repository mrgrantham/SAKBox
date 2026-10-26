#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>

#include <cstddef>
#include <cstring>

// The rendered sounds, compiled in (see BUILD: embed_file), as bytes:
// little-endian 16-bit samples, as the DS and 3DS (and desktops) hold them
namespace ScratchPadd::Audio::MadeSoundsPcm16::Bytes {
extern const unsigned char chime[], knock[], blast[], note[];
extern const std::size_t chimeSize, knockSize, blastSize, noteSize;
} // namespace ScratchPadd::Audio::MadeSoundsPcm16::Bytes

namespace ScratchPadd::Audio::MadeSoundsPcm16 {

namespace {

std::vector<std::int16_t> samples(const unsigned char *bytes, std::size_t size) {
  std::vector<std::int16_t> result(size / sizeof(std::int16_t));
  std::memcpy(result.data(), bytes, result.size() * sizeof(std::int16_t));
  return result;
}

} // namespace

std::vector<std::int16_t> chime() { return samples(Bytes::chime, Bytes::chimeSize); }
std::vector<std::int16_t> knock() { return samples(Bytes::knock, Bytes::knockSize); }
std::vector<std::int16_t> blast() { return samples(Bytes::blast, Bytes::blastSize); }
std::vector<std::int16_t> note() { return samples(Bytes::note, Bytes::noteSize); }

} // namespace ScratchPadd::Audio::MadeSoundsPcm16
