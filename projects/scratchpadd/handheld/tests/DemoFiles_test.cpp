// The files the DS demo carries (written by write_demo_files when it's
// built) read back as the DS's AudioPadd reads them: each WAV in its format,
// and the MIDI tune with all its notes, about four seconds long
#include <ScratchPadd/Audio/MidiSequence.hpp>
#include <ScratchPadd/Audio/WavReader.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;

namespace {

const std::string kFolder = "projects/scratchpadd/handheld/";

std::vector<std::uint8_t> bytesOf(const std::string &path) {
  std::ifstream file(kFolder + path, std::ios::binary);
  EXPECT_TRUE(file.good()) << path;
  return {std::istreambuf_iterator<char>(file), {}};
}

Wav::Sound wav(const std::string &path) {
  const auto bytes = bytesOf(path);
  Wav::Sound sound;
  std::string error;
  EXPECT_TRUE(Wav::read(bytes.data(), bytes.size(), sound, error)) << path << ": " << error;
  return sound;
}

} // namespace

TEST(DemoFilesTest, SoundsInEachFormat) {
  const Wav::Sound chime = wav("sounds/chime.wav"), knock = wav("sounds/knock.wav"),
                   blast = wav("sounds/blast.wav");
  EXPECT_EQ(chime.bits, 16);
  EXPECT_EQ(knock.bits, 8);
  EXPECT_EQ(blast.bits, 16); // IMA-ADPCM, decoded
  for (const Wav::Sound *sound : {&chime, &knock, &blast}) {
    EXPECT_EQ(sound->rate, 16384u);
    EXPECT_EQ(sound->channels, 1);
    EXPECT_GT(sound->frames(), 4000u);
  }
}

TEST(DemoFilesTest, TuneIsAMidiFile) {
  const auto bytes = bytesOf("music/tune.mid");
  std::string error;
  const auto tune = readMidi(bytes.data(), bytes.size(), 1000, error); // times in ms
  ASSERT_TRUE(tune) << error;
  const auto notes = std::count_if(tune->events.begin(), tune->events.end(), [](const auto &e) {
    return e.event.kind() == MidiEvent::NoteOn && e.event.data2 > 0;
  });
  EXPECT_EQ(notes, 8 + 8 + 4 + 8); // melody, bass, kicks, hi-hats
  // Eight beats at 120 a minute, less the end: the reader ends a sequence at
  // its last event, the melody's last note letting go at 7.9 beats
  EXPECT_NEAR(tune->frames, 3950u, 5u);
}
