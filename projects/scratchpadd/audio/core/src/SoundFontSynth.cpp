#include <ScratchPadd/Audio/SoundFontSynth.hpp>

// With Ogg Vorbis, TinySoundFont also reads .sf3 SoundFonts
#ifdef SCRATCHPADD_AUDIO_SF3
#define STB_VORBIS_HEADER_ONLY
// (stb_vorbis is one .c file; with STB_VORBIS_HEADER_ONLY it gives just its
// declarations, as its documentation describes)
#include <stb/stb_vorbis.c> // NOLINT(bugprone-suspicious-include)
#endif
#include <ScratchPadd/Audio/RealTime.hpp>

#include <cstdlib>

// TinySoundFont's allocations, reported so that any made while mixing are
// counted (there should be none: see the constructor)
namespace {
void *reportedMalloc(std::size_t size) {
  ScratchPadd::Audio::noteAllocation();
  return std::malloc(size);
}
void *reportedRealloc(void *memory, std::size_t size) {
  ScratchPadd::Audio::noteAllocation();
  return std::realloc(memory, size);
}
} // namespace
#define TSF_MALLOC reportedMalloc
#define TSF_REALLOC reportedRealloc
#define TSF_FREE std::free
#define TSF_IMPLEMENTATION
#include <tinysoundfont/tsf.h>

namespace ScratchPadd::Audio {

namespace {
constexpr int kChannels = 16;
constexpr int kDrumChannel = 9; // General MIDI's channel 10
} // namespace

std::unique_ptr<SoundFontSynth> SoundFontSynth::load(const std::string &path,
                                                     std::uint32_t sampleRate,
                                                     std::uint32_t maxNotes, std::string &error) {
  return made(tsf_load_filename(path.c_str()), path, sampleRate, maxNotes, error);
}

std::unique_ptr<SoundFontSynth> SoundFontSynth::load(const void *data, std::size_t size,
                                                     std::uint32_t sampleRate,
                                                     std::uint32_t maxNotes, std::string &error) {
  return made(tsf_load_memory(data, static_cast<int>(size)), "those bytes", sampleRate, maxNotes,
              error);
}

std::unique_ptr<SoundFontSynth> SoundFontSynth::made(tsf *font, const std::string &source,
                                                     std::uint32_t sampleRate,
                                                     std::uint32_t maxNotes, std::string &error) {
  if (!font) {
    error = "can't read " + source + " as a SoundFont";
    return nullptr;
  }
  if (tsf_get_presetcount(font) == 0) {
    tsf_close(font);
    error = "no instruments in " + source;
    return nullptr;
  }
  return std::unique_ptr<SoundFontSynth>(new SoundFontSynth(font, sampleRate, maxNotes));
}

SoundFontSynth::SoundFontSynth(tsf *font, std::uint32_t sampleRate, std::uint32_t maxNotes)
    : font_(font) {
  // Everything TinySoundFont would otherwise allocate while playing is
  // allocated here, on the owner's thread: its voices (a fixed number, so
  // notes beyond it steal one rather than grow the list) and all 16
  // channels, each on General MIDI's first instrument (channel 10 on drums;
  // set from the last, so all 16 are allocated at once)
  tsf_set_output(font_, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), 0.0f);
  tsf_set_max_voices(font_, static_cast<int>(maxNotes));
  for (int channel = kChannels - 1; channel >= 0; --channel) {
    tsf_channel_set_presetnumber(font_, channel, 0, channel == kDrumChannel);
  }
}

SoundFontSynth::~SoundFontSynth() { tsf_close(font_); }

void SoundFontSynth::handle(const MidiEvent &event) noexcept {
  const int channel = event.channel();
  switch (event.kind()) {
  case MidiEvent::NoteOff:
    tsf_channel_note_off(font_, channel, event.data1);
    return;
  case MidiEvent::NoteOn:
    if (event.data2 == 0) { // a note on at velocity 0 is a note off
      tsf_channel_note_off(font_, channel, event.data1);
    } else {
      tsf_channel_note_on(font_, channel, event.data1, event.data2 / 127.0f);
    }
    return;
  case MidiEvent::ControlChange:
    tsf_channel_midi_control(font_, channel, event.data1, event.data2);
    return;
  case MidiEvent::ProgramChange:
    tsf_channel_set_presetnumber(font_, channel, event.data1, channel == kDrumChannel);
    return;
  case MidiEvent::PitchBend:
    tsf_channel_set_pitchwheel(font_, channel, event.data1 | (event.data2 << 7));
    return;
  default: // pressure: TinySoundFont doesn't use it
    return;
  }
}

void SoundFontSynth::render(float *stereo, std::uint32_t frames) noexcept {
  tsf_render_float(font_, stereo, static_cast<int>(frames), /*flag_mixing=*/1);
}

void SoundFontSynth::allNotesOff() noexcept { tsf_note_off_all(font_); }

std::uint32_t SoundFontSynth::notesSounding() const noexcept {
  return static_cast<std::uint32_t>(tsf_active_voice_count(font_));
}

int SoundFontSynth::presetCount() const { return tsf_get_presetcount(font_); }

} // namespace ScratchPadd::Audio
