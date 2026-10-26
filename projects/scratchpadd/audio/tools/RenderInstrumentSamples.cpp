// Renders the built-in instrument to WAV files, to listen to: each of
// General MIDI's 16 instrument families playing the same phrase, and the
// drums. With a SoundFont given, renders the same with it too, to compare.
//
//   bazel run //projects/scratchpadd/audio:render_instrument_samples
//
// writes them to .audio-samples/ in the repo (or the folder given as the
// first argument).
#include <ScratchPadd/Audio/Engine.hpp>
#include <ScratchPadd/Audio/MidiSequence.hpp>

#include <DataDepRetriever/DataDependencies.hpp>
#include <miniaudio/miniaudio.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;

namespace {

constexpr std::uint32_t kRate = 48000;

// General MIDI's instrument families: programs 0-7, 8-15, ...
constexpr const char *kFamilies[16] = {
    "piano",      "chromatic-percussion",
    "organ",      "guitar",
    "bass",       "strings",
    "ensemble",   "brass",
    "reed",       "pipe",
    "synth-lead", "synth-pad",
    "synth-fx",   "ethnic",
    "percussive", "sound-fx",
};

// Builds a sequence from (beat, event) pairs at 120 beats a minute
class Phrase {
public:
  void at(double beat, const MidiEvent &event) {
    sequence_->events.push_back(
        {.frame = static_cast<std::uint32_t>(beat * 0.5 * kRate), .event = event});
  }
  // Notes on and off together
  void note(int channel, int key, double beat, double beats, int velocity = 100) {
    at(beat, MidiEvent::noteOn(channel, key, velocity));
    at(beat + beats, MidiEvent::noteOff(channel, key));
  }
  // The sequence, its events in order, lasting `beats`
  std::shared_ptr<const MidiSequence> done(double beats) {
    std::stable_sort(sequence_->events.begin(), sequence_->events.end(),
                     [](const auto &a, const auto &b) { return a.frame < b.frame; });
    sequence_->frames = static_cast<std::uint32_t>(beats * 0.5 * kRate);
    return sequence_;
  }

private:
  std::shared_ptr<MidiSequence> sequence_ = std::make_shared<MidiSequence>();
};

// A family's phrase: a rising arpeggio (each note's attack), then a chord
// held for two seconds (whether the sound holds or dies away) and let go
// (its release)
std::shared_ptr<const MidiSequence> familyPhrase(int family) {
  Phrase phrase;
  const int base = family == 4 ? 36 : 60; // bass an octave and more lower
  phrase.at(0, MidiEvent::programChange(0, family * 8));
  const int arpeggio[] = {0, 4, 7, 12};
  for (int i = 0; i < 4; ++i) {
    phrase.note(0, base + arpeggio[i], i * 0.5, 0.45);
  }
  for (int interval : {0, 4, 7}) {
    phrase.note(0, base + interval, 2.5, 4.0);
  }
  return phrase.done(9.0); // two seconds after letting go, for the release
}

// The drums: each sound alone, then a two-bar groove
std::shared_ptr<const MidiSequence> drumPhrase() {
  Phrase phrase;
  double beat = 0;
  // Kick, snare, closed and open hi-hat, low/mid/high toms, crash, ride, clap
  for (int key : {36, 38, 42, 46, 41, 45, 48, 49, 51, 39}) {
    phrase.note(9, key, beat, 0.25, 110);
    beat += 1.0;
  }
  beat += 1.0;
  for (int bar = 0; bar < 2; ++bar) {
    for (int eighth = 0; eighth < 8; ++eighth) {
      const double at = beat + eighth * 0.5;
      phrase.note(9, 42, at, 0.25, eighth % 2 ? 60 : 90);
      if (eighth == 0 || eighth == 5) {
        phrase.note(9, 36, at, 0.25, 115);
      }
      if (eighth == 2 || eighth == 6) {
        phrase.note(9, 38, at, 0.25, 105);
      }
    }
    beat += 4.0;
  }
  phrase.note(9, 49, beat, 0.25, 110); // a crash to finish
  return phrase.done(beat + 6.0);
}

// Plays `sequence` on `engine` until it ends and writes it to `path` as a
// 16-bit stereo WAV
bool render(Engine &engine, OfflineOutput &output, std::shared_ptr<const MidiSequence> sequence,
            const std::string &path) {
  std::string error;
  if (!engine.playMidi(std::move(sequence), {}, error)) {
    std::fprintf(stderr, "%s: %s\n", path.c_str(), error.c_str());
    return false;
  }
  ma_encoder_config config =
      ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 2, kRate);
  ma_encoder encoder;
  if (ma_encoder_init_file(path.c_str(), &config, &encoder) != MA_SUCCESS) {
    std::fprintf(stderr, "can't write %s\n", path.c_str());
    return false;
  }
  constexpr std::uint32_t kBlock = 1024;
  std::vector<float> mixed(2 * kBlock);
  std::vector<std::int16_t> samples(2 * kBlock);
  bool playing = true;
  while (playing) {
    output.render(mixed.data(), kBlock);
    for (std::size_t i = 0; i < mixed.size(); ++i) {
      samples[i] = static_cast<std::int16_t>(std::clamp(mixed[i], -1.0f, 1.0f) * 32767.0f);
    }
    ma_encoder_write_pcm_frames(&encoder, samples.data(), kBlock, nullptr);
    playing = engine.update().empty();
  }
  ma_encoder_uninit(&encoder);
  return true;
}

} // namespace

int main(int argc, char **argv) {
  DataDepRetriever::ConfigureDependencies(argv[0]);
  // `bazel run` starts in the runfiles; write to the repo itself
  const char *workspace = std::getenv("BUILD_WORKSPACE_DIRECTORY");
  const std::filesystem::path folder =
      argc > 1 ? std::filesystem::path(argv[1])
               : std::filesystem::path(workspace ? workspace : ".") / ".audio-samples";
  std::filesystem::create_directories(folder);

  // Each instrument: the built-in one, and the SoundFont Bazel downloads
  // for the demo, if it's there
  struct Instrument {
    std::string prefix;
    std::optional<std::string> soundFont;
  };
  std::vector<Instrument> instruments = {{"built-in", std::nullopt}};
  if (const auto font = DataDepRetriever::GetFullDependencyPath(
          "fluidr3_mono_soundfont/file/FluidR3Mono_GM.sf3")) {
    instruments.push_back({"fluidr3", font});
  }

  OfflineOutput output(kRate);
  Engine engine(output);
  std::string error;
  engine.start(error);
  int written = 0;
  for (const Instrument &instrument : instruments) {
    // (Each phrase starts by choosing its program, so one instrument plays
    // them all in turn)
    if (instrument.soundFont) {
      std::printf("Loading %s...\n", instrument.soundFont->c_str());
      const LoadResult loaded = engine.loadSoundFont(*instrument.soundFont);
      if (!loaded.ok) {
        std::fprintf(stderr, "%s\n", loaded.error.c_str());
        continue;
      }
    } else {
      engine.useBuiltInInstrument();
    }
    for (int family = 0; family < 16; ++family) {
      char name[64];
      std::snprintf(name, sizeof name, "%s-%02d-%s.wav", instrument.prefix.c_str(), family + 1,
                    kFamilies[family]);
      written += render(engine, output, familyPhrase(family), (folder / name).string());
    }
    written += render(engine, output, drumPhrase(),
                      (folder / (instrument.prefix + "-drums.wav")).string());
  }
  std::printf("Wrote %d files to %s\n", written, folder.string().c_str());
  return 0;
}
