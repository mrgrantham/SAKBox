#include <ScratchPadd/Audio/WavReader.hpp>

#include <algorithm>
#include <cstring>

namespace ScratchPadd::Audio::Wav {

namespace {

constexpr std::uint16_t kPcm = 1;
constexpr std::uint16_t kImaAdpcm = 0x11;
// IMA-ADPCM: frames in each block of `kAdpcmBlockBytes` (mono): the
// block's 4-byte header holds the first sample, then two samples a byte
constexpr std::uint16_t kAdpcmBlockBytes = 256;
constexpr std::uint16_t kAdpcmBlockFrames = 1 + (kAdpcmBlockBytes - 4) * 2;

constexpr int kStepSizes[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr int kIndexChanges[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

std::uint16_t u16(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t u32(const std::uint8_t *p) {
  return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16)) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}
void put16(std::vector<std::uint8_t> &out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void put32(std::vector<std::uint8_t> &out, std::uint32_t value) {
  put16(out, value & 0xFFFF);
  put16(out, value >> 16);
}

// One IMA-ADPCM step: the next sample from `nibble`, updating the state
struct AdpcmState {
  int predicted = 0;
  int index = 0;

  std::int16_t decode(int nibble) {
    const int step = kStepSizes[index];
    int difference = step >> 3;
    if (nibble & 4)
      difference += step;
    if (nibble & 2)
      difference += step >> 1;
    if (nibble & 1)
      difference += step >> 2;
    predicted = std::clamp(predicted + ((nibble & 8) ? -difference : difference), -32768, 32767);
    index = std::clamp(index + kIndexChanges[nibble], 0, 88);
    return static_cast<std::int16_t>(predicted);
  }

  // The nibble that comes nearest `sample`, decoding it as the reader will
  int encode(int sample) {
    const int step = kStepSizes[index];
    int difference = sample - predicted;
    int nibble = 0;
    if (difference < 0) {
      nibble = 8;
      difference = -difference;
    }
    if (difference >= step) {
      nibble |= 4;
      difference -= step;
    }
    if (difference >= step >> 1) {
      nibble |= 2;
      difference -= step >> 1;
    }
    if (difference >= step >> 2) {
      nibble |= 1;
    }
    decode(nibble);
    return nibble;
  }
};

bool readImaAdpcm(const std::uint8_t *data, std::uint32_t size, std::uint16_t channels,
                  std::uint16_t blockAlign, Sound &sound, std::string &error) {
  if (channels != 1 || blockAlign < 5) {
    error = "IMA-ADPCM WAV: only mono is read";
    return false;
  }
  sound.bits = 16;
  for (std::uint32_t block = 0; block + 4 <= size; block += blockAlign) {
    const std::uint32_t end = std::min<std::uint32_t>(block + blockAlign, size);
    AdpcmState state{.predicted = static_cast<std::int16_t>(u16(data + block)),
                     .index = std::min<int>(data[block + 2], 88)};
    sound.samples16.push_back(static_cast<std::int16_t>(state.predicted));
    for (std::uint32_t i = block + 4; i < end; ++i) {
      sound.samples16.push_back(state.decode(data[i] & 0x0F));
      sound.samples16.push_back(state.decode(data[i] >> 4));
    }
  }
  return true;
}

std::vector<std::uint8_t> header(std::uint16_t format, std::uint32_t rate, std::uint16_t channels,
                                 std::uint16_t bits, std::uint16_t blockAlign,
                                 std::uint32_t bytesPerSecond, std::uint32_t dataBytes,
                                 const std::vector<std::uint8_t> &extra) {
  std::vector<std::uint8_t> out = {'R', 'I', 'F', 'F'};
  const auto fmtBytes = static_cast<std::uint32_t>(16 + (extra.empty() ? 0 : 2 + extra.size()));
  put32(out, 4 + 8 + fmtBytes + 8 + dataBytes);
  out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  put32(out, fmtBytes);
  put16(out, format);
  put16(out, channels);
  put32(out, rate);
  put32(out, bytesPerSecond);
  put16(out, blockAlign);
  put16(out, bits);
  if (!extra.empty()) {
    put16(out, static_cast<std::uint32_t>(extra.size()));
    out.insert(out.end(), extra.begin(), extra.end());
  }
  out.insert(out.end(), {'d', 'a', 't', 'a'});
  put32(out, dataBytes);
  return out;
}

} // namespace

bool read(const std::uint8_t *data, std::size_t size, Sound &sound, std::string &error) {
  sound = {};
  if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
    error = "not a WAV file";
    return false;
  }
  std::uint16_t format = 0, blockAlign = 0;
  bool haveFormat = false;
  // The chunks: "fmt " says what the samples are, "data" holds them
  for (std::size_t at = 12; at + 8 <= size;) {
    const std::uint32_t length = u32(data + at + 4);
    const std::uint8_t *body = data + at + 8;
    const std::size_t available = std::min<std::size_t>(length, size - at - 8);
    if (std::memcmp(data + at, "fmt ", 4) == 0 && available >= 16) {
      format = u16(body);
      sound.channels = u16(body + 2);
      sound.rate = u32(body + 4);
      blockAlign = u16(body + 12);
      sound.bits = u16(body + 14);
      haveFormat = true;
    } else if (std::memcmp(data + at, "data", 4) == 0) {
      if (!haveFormat || sound.channels == 0) {
        error = "WAV file has no format before its samples";
        return false;
      }
      const auto bytes = static_cast<std::uint32_t>(available);
      if (format == kImaAdpcm) {
        return readImaAdpcm(body, bytes, sound.channels, blockAlign, sound, error);
      }
      if (format != kPcm || (sound.bits != 8 && sound.bits != 16)) {
        error = "WAV file isn't 8- or 16-bit PCM or IMA-ADPCM";
        return false;
      }
      if (sound.bits == 8) {
        // WAV's 8-bit samples are unsigned, around 128
        sound.samples8.resize(bytes);
        for (std::uint32_t i = 0; i < bytes; ++i) {
          sound.samples8[i] = static_cast<std::int8_t>(body[i] - 128);
        }
      } else {
        sound.samples16.resize(bytes / 2);
        for (std::uint32_t i = 0; i < bytes / 2; ++i) {
          sound.samples16[i] = static_cast<std::int16_t>(u16(body + 2 * i));
        }
      }
      return true;
    }
    at += 8 + length + (length & 1); // chunks are padded to even lengths
  }
  error = "WAV file has no samples";
  return false;
}

std::vector<std::uint8_t> write16(const std::vector<std::int16_t> &interleaved, std::uint32_t rate,
                                  std::uint16_t channels) {
  const auto bytes = static_cast<std::uint32_t>(interleaved.size() * 2);
  auto out = header(kPcm, rate, channels, 16, static_cast<std::uint16_t>(2 * channels),
                    rate * 2 * channels, bytes, {});
  for (std::int16_t sample : interleaved) {
    put16(out, static_cast<std::uint16_t>(sample));
  }
  return out;
}

std::vector<std::uint8_t> write8(const std::vector<std::int8_t> &interleaved, std::uint32_t rate,
                                 std::uint16_t channels) {
  auto out = header(kPcm, rate, channels, 8, channels, rate * channels,
                    static_cast<std::uint32_t>(interleaved.size()), {});
  for (std::int8_t sample : interleaved) {
    out.push_back(static_cast<std::uint8_t>(sample + 128));
  }
  if (interleaved.size() & 1) {
    out.push_back(0); // pad the chunk to an even length
  }
  return out;
}

std::vector<std::uint8_t> writeImaAdpcm(const std::vector<std::int16_t> &mono, std::uint32_t rate) {
  std::vector<std::uint8_t> blocks;
  AdpcmState state;
  for (std::size_t start = 0; start < mono.size(); start += kAdpcmBlockFrames) {
    // Each block starts from its first sample, as it is
    state.predicted = mono[start];
    put16(blocks, static_cast<std::uint16_t>(mono[start]));
    blocks.push_back(static_cast<std::uint8_t>(state.index));
    blocks.push_back(0);
    for (std::size_t i = start + 1; i < start + kAdpcmBlockFrames; i += 2) {
      const int low = state.encode(i < mono.size() ? mono[i] : 0);
      const int high = state.encode(i + 1 < mono.size() ? mono[i + 1] : 0);
      blocks.push_back(static_cast<std::uint8_t>(low | (high << 4)));
    }
  }
  std::vector<std::uint8_t> framesPerBlock;
  put16(framesPerBlock, kAdpcmBlockFrames);
  auto out =
      header(kImaAdpcm, rate, 1, 4, kAdpcmBlockBytes, rate * kAdpcmBlockBytes / kAdpcmBlockFrames,
             static_cast<std::uint32_t>(blocks.size()), framesPerBlock);
  out.insert(out.end(), blocks.begin(), blocks.end());
  return out;
}

} // namespace ScratchPadd::Audio::Wav
