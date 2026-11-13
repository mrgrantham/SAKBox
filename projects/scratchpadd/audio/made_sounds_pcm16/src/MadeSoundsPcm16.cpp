#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>

#include <cstddef>
#include <cstring>

// The rendered sounds, compiled in (see BUILD: embed_file), as bytes:
// little-endian 16-bit samples, as the DS and 3DS (and desktops) hold them
namespace ScratchPadd::Audio::MadeSoundsPcm16::Bytes {
extern const unsigned char kChime[], kKnock[], kBlast[], kNote[];
extern const std::size_t kChimeSize, kKnockSize, kBlastSize, kNoteSize;
} // namespace ScratchPadd::Audio::MadeSoundsPcm16::Bytes

namespace ScratchPadd::Audio::MadeSoundsPcm16 {

namespace {

std::vector<std::int16_t> samples(const unsigned char *bytes, std::size_t size) {
  std::vector<std::int16_t> result(size / sizeof(std::int16_t));
  std::memcpy(result.data(), bytes, result.size() * sizeof(std::int16_t));
  return result;
}

} // namespace

std::vector<std::int16_t> chime() { return samples(Bytes::kChime, Bytes::kChimeSize); }
std::vector<std::int16_t> knock() { return samples(Bytes::kKnock, Bytes::kKnockSize); }
std::vector<std::int16_t> blast() { return samples(Bytes::kBlast, Bytes::kBlastSize); }
std::vector<std::int16_t> note() { return samples(Bytes::kNote, Bytes::kNoteSize); }

} // namespace ScratchPadd::Audio::MadeSoundsPcm16
