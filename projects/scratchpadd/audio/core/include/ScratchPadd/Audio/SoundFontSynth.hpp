#pragma once

#include <ScratchPadd/Audio/Synth.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct tsf; // TinySoundFont's synthesizer

namespace ScratchPadd::Audio {

// Plays MIDI with the instruments of a SoundFont (.sf2; also .sf3, the
// Ogg-compressed kind, where built with Ogg Vorbis: the desktop builds),
// using TinySoundFont. Loading reads every sample into memory as floats, so
// a General MIDI SoundFont can take a few hundred megabytes.
class SoundFontSynth final : public Synth {
public:
  // Loads a SoundFont to play at `sampleRate`, with up to `maxNotes`
  // sounding at once (more steal the quietest released one). Returns
  // nothing, with the reason in `error`, if it can't.
  static std::unique_ptr<SoundFontSynth> load(const std::string &path, std::uint32_t sampleRate,
                                              std::uint32_t maxNotes, std::string &error);
  // The same, from the file's bytes (copied)
  static std::unique_ptr<SoundFontSynth> load(const void *data, std::size_t size,
                                              std::uint32_t sampleRate, std::uint32_t maxNotes,
                                              std::string &error);
  ~SoundFontSynth() override;

  SoundFontSynth(const SoundFontSynth &) = delete;
  SoundFontSynth &operator=(const SoundFontSynth &) = delete;

  void handle(const MidiEvent &event) noexcept override;
  void render(float *stereo, std::uint32_t frames) noexcept override;
  void allNotesOff() noexcept override;
  std::uint32_t notesSounding() const noexcept override;

  // How many instruments (presets) it has
  int presetCount() const;

private:
  SoundFontSynth(tsf *font, std::uint32_t sampleRate, std::uint32_t maxNotes);
  static std::unique_ptr<SoundFontSynth> made(tsf *font, const std::string &source,
                                              std::uint32_t sampleRate, std::uint32_t maxNotes,
                                              std::string &error);
  tsf *font_;
};

} // namespace ScratchPadd::Audio
