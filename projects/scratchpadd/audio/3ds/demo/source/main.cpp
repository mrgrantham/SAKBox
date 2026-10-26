// ScratchPadd audio on a 3DS, through its DSP: a tune on the built-in
// instrument (MIDI made in code, looping) and a chime every two seconds,
// playing by themselves. The top screen shows how the output is doing; the
// figures are also written to sdmc:/scratchpadd-audio-3ds-demo.txt each
// second. The bottom screen lists the buttons.
#include <3ds.h>

#include <ScratchPadd/Audio/Engine.hpp>
#include <ScratchPadd/Audio/NdspOutput.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;

namespace {

constexpr const char *kSoundFontPath = "sdmc:/3ds/scratchpadd/instrument.sf2";
constexpr const char *kFiguresPath = "sdmc:/scratchpadd-audio-3ds-demo.txt";

// A soft bell: two sine partials dying away over 0.8 s, mono
std::vector<float> chime(std::uint32_t rate) {
  std::vector<float> samples(rate * 8 / 10);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(rate);
    const float attack = std::min(1.0f, t * 200.0f);
    samples[i] = attack * std::exp(-5.0f * t) *
                 (0.6f * std::sin(6.2831853f * 880.0f * t) +
                  0.25f * std::sin(6.2831853f * 2.76f * 880.0f * t));
  }
  return samples;
}

// Four seconds of music at 120 beats a minute: a vibraphone tune, a bass
// line, and kick and hi-hats on General MIDI's drum channel
std::shared_ptr<MidiSequence> tune(std::uint32_t rate) {
  auto sequence = std::make_shared<MidiSequence>();
  const auto at = [&](double beat, MidiEvent event) {
    sequence->events.push_back(
        {.frame = static_cast<std::uint32_t>(beat * 0.5 * rate), .event = event});
  };
  at(0, MidiEvent::programChange(0, 11)); // vibraphone
  at(0, MidiEvent::programChange(1, 33)); // electric bass
  constexpr int kMelody[] = {72, 76, 79, 76, 74, 77, 81, 77};
  constexpr int kBass[] = {48, 48, 53, 53, 50, 50, 55, 55};
  for (int beat = 0; beat < 8; ++beat) {
    at(beat, MidiEvent::noteOn(0, kMelody[beat], 90));
    at(beat + 0.9, MidiEvent::noteOff(0, kMelody[beat]));
    at(beat, MidiEvent::noteOn(1, kBass[beat], 100));
    at(beat + 0.45, MidiEvent::noteOff(1, kBass[beat]));
    if (beat % 2 == 0) {
      at(beat, MidiEvent::noteOn(9, 36, 110)); // kick
    }
    at(beat, MidiEvent::noteOn(9, 42, 60)); // closed hi-hat
    at(beat + 0.5, MidiEvent::noteOn(9, 42, 40));
  }
  std::stable_sort(sequence->events.begin(), sequence->events.end(),
                   [](const auto &a, const auto &b) { return a.frame < b.frame; });
  sequence->frames = 4 * rate;
  return sequence;
}

double milliseconds(std::uint32_t microseconds) { return microseconds / 1000.0; }

} // namespace

int main() {
  gfxInitDefault();
  PrintConsole top, bottom;
  consoleInit(GFX_TOP, &top);
  consoleInit(GFX_BOTTOM, &bottom);

  consoleSelect(&bottom);
  std::printf(
      "\x1b[1mScratchPadd audio on the 3DS\x1b[0m\n\n"
      "Plays by itself: a tune on the\nbuilt-in instrument, and a chime\nevery two seconds.\n\n"
      "A      a chime now\n"
      "B      music on or off\n"
      "Y      instrument from the SD card:\n%s\n"
      "X      the built-in instrument\n"
      "START  quit\n",
      kSoundFontPath);
  consoleSelect(&top);

  NdspOutput output;
  Engine engine(output, {.maxVoices = 32, .instrumentNotes = 24});
  std::string error;
  if (!engine.start(error)) {
    std::printf("\x1b[31mNo sound: %s\x1b[0m\n\nPress START to quit.\n", error.c_str());
    if (FILE *figures = std::fopen(kFiguresPath, "w")) {
      std::fprintf(figures, "No sound: %s\n", error.c_str());
      std::fclose(figures);
    }
    while (aptMainLoop()) {
      hidScanInput();
      if (hidKeysDown() & KEY_START)
        break;
      gspWaitForVBlank();
    }
    gfxExit();
    return 0;
  }

  const std::uint32_t rate = engine.sampleRate();
  const std::vector<float> bell = chime(rate);
  engine.loadSamples("chime", bell.data(), bell.size(), rate, 1);
  const std::shared_ptr<const MidiSequence> music = tune(rate);
  std::optional<VoiceId> musicVoice = engine.playMidi(music, {.volume = 0.8f, .loop = true}, error);
  std::string instrument = "built-in";
  std::uint32_t chimes = 0;
  std::uint32_t frame = 0;

  while (aptMainLoop()) {
    hidScanInput();
    const u32 down = hidKeysDown();
    if (down & KEY_START)
      break;

    const bool everyTwoSeconds = frame % 120 == 0;
    if ((down & KEY_A) || everyTwoSeconds) {
      engine.play("chime", {.volume = 0.5f, .pan = chimes % 2 ? 0.6f : -0.6f});
      ++chimes;
    }
    if (down & KEY_B) {
      if (musicVoice && engine.isPlaying(*musicVoice)) {
        engine.stop(*musicVoice);
        musicVoice.reset();
      } else {
        musicVoice = engine.playMidi(music, {.volume = 0.8f, .loop = true}, error);
      }
    }
    if (down & KEY_Y) {
      const LoadResult loaded = engine.loadSoundFont(kSoundFontPath);
      instrument = loaded.ok ? "SoundFont" : "built-in (" + loaded.error + ")";
    }
    if (down & KEY_X) {
      engine.useBuiltInInstrument();
      instrument = "built-in";
    }
    engine.update();

    // The figures, four times a second on screen and once a second to the
    // SD card
    if (frame % 15 == 0) {
      const NdspStats &out = output.stats();
      const MixerStats &mix = engine.stats();
      char text[640];
      std::snprintf(text, sizeof text,
                    "ScratchPadd audio on the 3DS\n\n"
                    "Output        %lu Hz, %d x %lu-frame buffers\n"
                    "Buffers       %lu queued, %lu mixed\n"
                    "Underruns     %lu\n"
                    "Mix time      %.2f ms last, %.2f ms longest\n"
                    "              (a buffer lasts %.1f ms)\n"
                    "Voices        %lu playing\n"
                    "Instrument    %s, %lu notes\n"
                    "Music         %s\n"
                    "Chimes        %lu\n"
                    "Running       %lu s\n",
                    static_cast<unsigned long>(rate), NdspOutput::kBuffers,
                    static_cast<unsigned long>(NdspOutput::kBufferFrames),
                    static_cast<unsigned long>(out.buffersQueued.load()),
                    static_cast<unsigned long>(out.buffersMixed.load()),
                    static_cast<unsigned long>(out.underruns.load()),
                    milliseconds(out.lastMixMicroseconds.load()),
                    milliseconds(out.maxMixMicroseconds.load()),
                    1000.0 * NdspOutput::kBufferFrames / rate,
                    static_cast<unsigned long>(mix.voicesPlaying.load()), instrument.c_str(),
                    static_cast<unsigned long>(mix.notesSounding.load()),
                    musicVoice && engine.isPlaying(*musicVoice) ? "playing" : "stopped",
                    static_cast<unsigned long>(chimes), static_cast<unsigned long>(frame / 60));
      // Each line cleared to its end, as it may be shorter than last time
      std::printf("\x1b[1;1H");
      for (const char *line = text; *line;) {
        const char *end = std::strchr(line, '\n');
        const int length = end ? static_cast<int>(end - line) : static_cast<int>(std::strlen(line));
        std::printf("%.*s\x1b[K\n", length, line);
        line += length + (end ? 1 : 0);
      }
      if (frame % 60 == 0) {
        if (FILE *figures = std::fopen(kFiguresPath, "w")) {
          std::fputs(text, figures);
          std::fclose(figures);
        }
      }
    }
    ++frame;
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
  }
  engine.stop();
  gfxExit();
  return 0;
}
