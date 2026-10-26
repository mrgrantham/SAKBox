// The integer WAV reader (the DS's), on files it writes and on edge cases
#include <ScratchPadd/Audio/WavReader.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;

namespace {

Wav::Sound readBack(const std::vector<std::uint8_t> &bytes) {
  Wav::Sound sound;
  std::string error;
  EXPECT_TRUE(Wav::read(bytes.data(), bytes.size(), sound, error)) << error;
  return sound;
}

// A 440 Hz sine, half full scale, `frames` long at 16 kHz
std::vector<std::int16_t> sine(std::size_t frames) {
  std::vector<std::int16_t> samples(frames);
  for (std::size_t i = 0; i < frames; ++i) {
    samples[i] = static_cast<std::int16_t>(16000 * std::sin(6.2831853 * 440 * i / 16000.0));
  }
  return samples;
}

} // namespace

TEST(WavReaderTest, SixteenBitAsWritten) {
  const std::vector<std::int16_t> stereo = {0, 1, -1, 32767, -32768, 1234};
  const Wav::Sound sound = readBack(Wav::write16(stereo, 22050, 2));
  EXPECT_EQ(sound.rate, 22050u);
  EXPECT_EQ(sound.channels, 2);
  EXPECT_EQ(sound.bits, 16);
  EXPECT_EQ(sound.samples16, stereo);
  EXPECT_EQ(sound.frames(), 3u);
}

// 8-bit WAV samples are unsigned; they come back signed, as the DS plays them
TEST(WavReaderTest, EightBitAsWritten) {
  const std::vector<std::int8_t> mono = {0, 1, -1, 127, -128};
  const Wav::Sound sound = readBack(Wav::write8(mono, 8000, 1));
  EXPECT_EQ(sound.bits, 8);
  EXPECT_EQ(sound.samples8, mono);
}

// IMA-ADPCM decodes close to what was encoded (it keeps 4 bits a sample),
// whole blocks of it
TEST(WavReaderTest, ImaAdpcmDecodesCloseToTheOriginal) {
  const std::vector<std::int16_t> original = sine(2000);
  const auto bytes = Wav::writeImaAdpcm(original, 16000);
  EXPECT_LT(bytes.size(), original.size() * 2 / 3); // smaller than 16-bit by far
  const Wav::Sound sound = readBack(bytes);
  EXPECT_EQ(sound.bits, 16);
  ASSERT_GE(sound.samples16.size(), original.size()); // (padded to whole blocks)
  double error = 0;
  for (std::size_t i = 0; i < original.size(); ++i) {
    error += std::abs(sound.samples16[i] - original[i]);
  }
  EXPECT_LT(error / original.size(), 300); // about 2% of the signal
}

// Chunks it doesn't know (here a "LIST" before the samples) are skipped
TEST(WavReaderTest, SkipsOtherChunks) {
  auto bytes = Wav::write16({1, 2, 3}, 8000, 1);
  const std::vector<std::uint8_t> list = {'L', 'I', 'S', 'T', 3, 0, 0, 0, 'a', 'b', 'c', 0};
  const auto data =
      std::search(bytes.begin(), bytes.end(), std::begin("data"), std::end("data") - 1);
  bytes.insert(data, list.begin(), list.end());
  EXPECT_EQ(readBack(bytes).samples16, (std::vector<std::int16_t>{1, 2, 3}));
}

TEST(WavReaderTest, RefusesWhatItCantRead) {
  Wav::Sound sound;
  std::string error;
  const std::vector<std::uint8_t> junk = {1, 2, 3, 4};
  EXPECT_FALSE(Wav::read(junk.data(), junk.size(), sound, error));
  EXPECT_EQ(error, "not a WAV file");
  auto floatWav = Wav::write16({1, 2}, 8000, 1);
  floatWav[20] = 3; // format 3: 32-bit float
  EXPECT_FALSE(Wav::read(floatWav.data(), floatWav.size(), sound, error));
  EXPECT_EQ(error, "WAV file isn't 8- or 16-bit PCM or IMA-ADPCM");
}
