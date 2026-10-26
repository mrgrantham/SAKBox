#pragma once

// MIDI files and SoundFonts made in code, for tests (here and on the 3DS),
// so no binary files need to live in the repo.

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace MidiTestData {

using Bytes = std::vector<std::uint8_t>;

inline void putBig(Bytes &out, std::uint32_t value, int bytes) {
  for (int i = bytes - 1; i >= 0; --i)
    out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
inline void putLittle(Bytes &out, std::uint32_t value, int bytes) {
  for (int i = 0; i < bytes; ++i)
    out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

// One event of a MIDI file: `tick` (480 to a beat, at 120 beats a minute:
// 960 ticks a second) and its bytes
struct TimedBytes {
  std::uint32_t tick;
  std::uint8_t status, data1, data2;
};

// A standard MIDI file (format 0) playing `events` at 120 beats a minute
inline Bytes midiFile(const std::vector<TimedBytes> &events) {
  Bytes track;
  const auto delta = [&](std::uint32_t ticks) { // variable-length quantity
    std::uint8_t groups[5];
    int count = 0;
    do {
      groups[count++] = static_cast<std::uint8_t>(ticks & 0x7F);
      ticks >>= 7;
    } while (ticks);
    while (count-- > 0)
      track.push_back(static_cast<std::uint8_t>(groups[count] | (count ? 0x80 : 0)));
  };
  // Tempo: 500,000 microseconds a beat
  delta(0);
  track.insert(track.end(), {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});
  std::uint32_t now = 0;
  for (const TimedBytes &event : events) {
    delta(event.tick - now);
    now = event.tick;
    track.push_back(event.status);
    track.push_back(event.data1);
    // Program change and channel pressure have one data byte
    if ((event.status & 0xF0) != 0xC0 && (event.status & 0xF0) != 0xD0)
      track.push_back(event.data2);
  }
  delta(0);
  track.insert(track.end(), {0xFF, 0x2F, 0x00}); // end of track

  Bytes file = {'M', 'T', 'h', 'd'};
  putBig(file, 6, 4);
  putBig(file, 0, 2);   // format 0
  putBig(file, 1, 2);   // one track
  putBig(file, 480, 2); // ticks a beat
  file.insert(file.end(), {'M', 'T', 'r', 'k'});
  putBig(file, static_cast<std::uint32_t>(track.size()), 4);
  file.insert(file.end(), track.begin(), track.end());
  return file;
}

// A SoundFont (.sf2) with one instrument, General MIDI's first (bank 0,
// preset 0): a looped sine at 440 Hz, recorded at key 69 (A4), so key 69
// plays 440 Hz. Its envelope is the SoundFont default: instant attack,
// full sustain, instant release.
inline Bytes soundFont() {
  constexpr std::uint32_t kRate = 44000;
  constexpr std::uint32_t kLength = 1000; // ten cycles of 100 samples
  const auto chunk = [](const char *id, const Bytes &body) {
    Bytes out(id, id + 4);
    putLittle(out, static_cast<std::uint32_t>(body.size()), 4);
    out.insert(out.end(), body.begin(), body.end());
    if (body.size() % 2)
      out.push_back(0);
    return out;
  };
  const auto list = [&](const char *type, const std::vector<Bytes> &chunks) {
    Bytes body(type, type + 4);
    for (const Bytes &c : chunks)
      body.insert(body.end(), c.begin(), c.end());
    return chunk("LIST", body);
  };
  const auto name = [](Bytes &out, const std::string &text) { // 20 bytes
    for (std::size_t i = 0; i < 20; ++i)
      out.push_back(i < text.size() ? static_cast<std::uint8_t>(text[i]) : 0);
  };

  Bytes ifil, inam;
  putLittle(ifil, 2, 2);
  putLittle(ifil, 1, 2);
  inam = {'s', 'i', 'n', 'e', 0, 0};

  Bytes smpl;
  for (std::uint32_t i = 0; i < kLength + 46; ++i) { // 46 silent samples after, as SF2 asks
    const double value = i < kLength ? std::sin(6.283185307179586 * i / 100.0) : 0.0;
    putLittle(smpl, static_cast<std::uint16_t>(static_cast<std::int16_t>(value * 32000)), 2);
  }

  // The "hydra": presets, instruments and samples, each list ending with a
  // terminal record
  Bytes phdr, pbag, pmod(10, 0), pgen, inst, ibag, imod(10, 0), igen, shdr;
  for (int i = 0; i < 2; ++i) { // the preset, then the terminal "EOP"
    name(phdr, i == 0 ? "Sine" : "EOP");
    putLittle(phdr, 0, 2);     // preset 0
    putLittle(phdr, 0, 2);     // bank 0
    putLittle(phdr, i, 2);     // its first bag
    putLittle(phdr, 0, 4 * 3); // library, genre, morphology
  }
  putLittle(pbag, 0, 2), putLittle(pbag, 0, 2);  // the preset's bag: generators from 0
  putLittle(pbag, 1, 2), putLittle(pbag, 0, 2);  // terminal
  putLittle(pgen, 41, 2), putLittle(pgen, 0, 2); // instrument 0
  putLittle(pgen, 0, 4);                         // terminal
  name(inst, "Sine"), putLittle(inst, 0, 2);
  name(inst, "EOI"), putLittle(inst, 1, 2);
  putLittle(ibag, 0, 2), putLittle(ibag, 0, 2);
  putLittle(ibag, 2, 2), putLittle(ibag, 0, 2);
  putLittle(igen, 54, 2), putLittle(igen, 1, 2); // sample modes: loop
  putLittle(igen, 53, 2), putLittle(igen, 0, 2); // sample 0 (last, as SF2 asks)
  putLittle(igen, 0, 4);                         // terminal
  for (int i = 0; i < 2; ++i) {                  // the sample, then the terminal "EOS"
    name(shdr, i == 0 ? "Sine" : "EOS");
    const bool sample = i == 0;
    putLittle(shdr, 0, 4);                    // start
    putLittle(shdr, sample ? kLength : 0, 4); // end
    putLittle(shdr, 0, 4);                    // loop start
    putLittle(shdr, sample ? kLength : 0, 4); // loop end
    putLittle(shdr, sample ? kRate : 0, 4);   // sample rate
    shdr.push_back(sample ? 69 : 0);          // recorded at key 69
    shdr.push_back(0);                        // pitch correction
    putLittle(shdr, 0, 2);                    // sample link
    putLittle(shdr, sample ? 1 : 0, 2);       // mono
  }

  Bytes body = {'s', 'f', 'b', 'k'};
  for (const Bytes &part :
       {list("INFO", {chunk("ifil", ifil), chunk("INAM", inam)}),
        list("sdta", {chunk("smpl", smpl)}),
        list("pdta", {chunk("phdr", phdr), chunk("pbag", pbag), chunk("pmod", pmod),
                      chunk("pgen", pgen), chunk("inst", inst), chunk("ibag", ibag),
                      chunk("imod", imod), chunk("igen", igen), chunk("shdr", shdr)})})
    body.insert(body.end(), part.begin(), part.end());
  return chunk("RIFF", body);
}

} // namespace MidiTestData
