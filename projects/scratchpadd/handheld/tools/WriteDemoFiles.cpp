// Writes the files the DS ScratchPadd demo carries in its file system
// (NitroFS), when it's built: the sounds made in code as WAV files, one in
// each format the DS's AudioPadd reads (16-bit, 8-bit and IMA-ADPCM), and a
// short MIDI tune.
//
//   write_demo_files FOLDER   writes FOLDER/sounds/{chime,knock,blast}.wav
//                             and FOLDER/music/tune.mid
#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/WavReader.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using namespace ScratchPadd::Audio;

constexpr std::uint32_t kRate = 16384;

void save(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary)
      .write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::int16_t> sixteenBit(const std::vector<float> &samples) {
  std::vector<std::int16_t> out(samples.size());
  std::transform(samples.begin(), samples.end(), out.begin(), [](float s) {
    return static_cast<std::int16_t>(std::lround(std::clamp(s, -1.0f, 1.0f) * 32767));
  });
  return out;
}

std::vector<std::int8_t> eightBit(const std::vector<float> &samples) {
  std::vector<std::int8_t> out(samples.size());
  std::transform(samples.begin(), samples.end(), out.begin(), [](float s) {
    return static_cast<std::int8_t>(std::lround(std::clamp(s, -1.0f, 1.0f) * 127));
  });
  return out;
}

// A standard MIDI file (format 0, one track, 480 ticks a beat at 120 beats
// a minute): a vibraphone tune over a bass line, with kick and hi-hats on
// the drum channel, eight beats long
std::vector<std::uint8_t> tune() {
  struct Event {
    std::uint32_t tick;
    std::vector<std::uint8_t> bytes;
  };
  std::vector<Event> events;
  constexpr std::uint32_t kBeat = 480;
  const auto note = [&](int channel, int key, double beat, double beats, int velocity) {
    const auto start = static_cast<std::uint32_t>(beat * kBeat);
    events.push_back({start,
                      {static_cast<std::uint8_t>(0x90 | channel), static_cast<std::uint8_t>(key),
                       static_cast<std::uint8_t>(velocity)}});
    events.push_back(
        {start + static_cast<std::uint32_t>(beats * kBeat),
         {static_cast<std::uint8_t>(0x80 | channel), static_cast<std::uint8_t>(key), 0}});
  };
  events.push_back({0, {0xC0, 11}}); // vibraphone
  events.push_back({0, {0xC1, 33}}); // electric bass
  constexpr int kMelody[] = {72, 76, 79, 76, 74, 77, 81, 77};
  constexpr int kBass[] = {48, 48, 53, 53, 50, 50, 55, 55};
  for (int beat = 0; beat < 8; ++beat) {
    note(0, kMelody[beat], beat, 0.9, 90);
    note(1, kBass[beat], beat, 0.45, 100);
    if (beat % 2 == 0) {
      note(9, 36, beat, 0.25, 110); // kick
    }
    note(9, 42, beat + 0.5, 0.25, 50); // hi-hat
  }
  std::stable_sort(events.begin(), events.end(),
                   [](const Event &a, const Event &b) { return a.tick < b.tick; });
  std::vector<std::uint8_t> track;
  const auto variableLength = [&](std::uint32_t value) {
    std::uint8_t bytes[4];
    int count = 0;
    do {
      bytes[count++] = value & 0x7F;
      value >>= 7;
    } while (value);
    while (count--) {
      track.push_back(static_cast<std::uint8_t>(bytes[count] | (count ? 0x80 : 0)));
    }
  };
  std::uint32_t last = 0;
  for (const Event &event : events) {
    variableLength(event.tick - last);
    last = event.tick;
    track.insert(track.end(), event.bytes.begin(), event.bytes.end());
  }
  variableLength(8 * kBeat - last);
  track.insert(track.end(), {0xFF, 0x2F, 0x00}); // end of track
  std::vector<std::uint8_t> file = {'M', 'T', 'h',        'd',          0,   0,   0,   6,  0, 0,
                                    0,   1,   kBeat >> 8, kBeat & 0xFF, 'M', 'T', 'r', 'k'};
  const auto size = static_cast<std::uint32_t>(track.size());
  file.insert(file.end(),
              {static_cast<std::uint8_t>(size >> 24), static_cast<std::uint8_t>(size >> 16),
               static_cast<std::uint8_t>(size >> 8), static_cast<std::uint8_t>(size)});
  file.insert(file.end(), track.begin(), track.end());
  return file;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s FOLDER\n", argv[0]);
    return 1;
  }
  const std::filesystem::path folder = argv[1];
  save(folder / "sounds/chime.wav", Wav::write16(sixteenBit(MadeSounds::chime(kRate)), kRate, 1));
  save(folder / "sounds/knock.wav", Wav::write8(eightBit(MadeSounds::knock(kRate)), kRate, 1));
  save(folder / "sounds/blast.wav",
       Wav::writeImaAdpcm(sixteenBit(MadeSounds::blast(kRate)), kRate));
  save(folder / "music/tune.mid", tune());
  return 0;
}
