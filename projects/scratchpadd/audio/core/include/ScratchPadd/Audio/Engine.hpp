#pragma once

#include <ScratchPadd/Audio/MidiSequence.hpp>
#include <ScratchPadd/Audio/Mixer.hpp>
#include <ScratchPadd/Audio/Output.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::Audio {

// What play() does when maxVoices are playing
enum class WhenVoicesFull : std::uint8_t {
  // Takes over the quietest sound playing (the oldest of the quietest),
  // which fades out over one block, unless the new one is quieter still:
  // then it's refused. Music and MIDI are never taken over. As game audio
  // does: a burst of sounds keeps the loudest, newest ones.
  StealQuietest,
  // Refuses the new sound
  Refuse,
};

struct EngineConfig {
  std::uint32_t maxVoices = 32;       // sounds and music playing at once
  std::uint32_t queueCapacity = 1024; // commands waiting for the audio thread
  double musicAheadSeconds = 0.5;     // music decoded ahead of what's playing
  std::uint32_t instrumentNotes = 48; // notes the instrument plays at once
  WhenVoicesFull whenVoicesFull = WhenVoicesFull::StealQuietest;
};

// What a load did: ok, or why not
struct LoadResult {
  bool ok = false;
  std::string error;
};

// The audio engine: plays named sounds and streamed music through an
// Output, mixed by a Mixer on the output's audio thread.
//
// Use it from one thread (the owner: AudioPadd's, say). That thread does
// all the slow work, decoding and resampling files when they're loaded and
// decoding music ahead as it plays, so the audio thread only mixes. The two
// threads talk through wait-free queues. Memory the audio thread may still
// be reading (a sound being unloaded, a stream that ended) is freed here,
// on the owner's thread, once the audio thread says it's done with it.
//
// Plain C++ with no other ScratchPadd dependency, so it builds anywhere the
// core does, the 3DS included. It doesn't throw.
class Engine {
public:
  Engine(Output &output, const EngineConfig &config = {});
  ~Engine();

  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  // Starts the output. Returns false, with the reason, if it can't start.
  bool start(std::string &error);
  void stop();

  // The rate everything is mixed at (the output's)
  std::uint32_t sampleRate() const { return sampleRate_; }

  // Loads a WAV, FLAC or MP3 file as sound `name`, replacing any sound of
  // that name, decoded and resampled to the engine's rate now
  LoadResult loadFile(const std::string &name, const std::string &path);

  // Loads `count` samples (interleaved, `channels` per frame, `rate` frames
  // a second) as sound `name`, converted to stereo at the engine's rate
  LoadResult loadSamples(const std::string &name, const float *samples, std::size_t count,
                         std::uint32_t rate, std::uint32_t channels);

  // Unloads sound `name`; voices playing it stop at once. False if there
  // was no such sound.
  bool unload(const std::string &name);
  bool hasSound(const std::string &name) const { return sounds_.contains(name); }

  // Plays sound `name`. Returns its voice, or nothing if there's no such
  // sound or every voice is in use.
  std::optional<VoiceId> play(const std::string &name, const VoiceSettings &settings = {});

  // Streams a music file (in the Music group), decoded as it plays. Returns
  // its voice, or nothing with the reason in `error`.
  std::optional<VoiceId> playMusic(const std::string &path, const VoiceSettings &settings,
                                   std::string &error);

  // Fades a voice out and stops it. False if it isn't playing.
  bool stop(VoiceId voice);
  // Changes how a voice plays (gains ramp to the new values). False if it
  // isn't playing.
  bool set(VoiceId voice, const VoiceSettings &settings);
  void stopAll();

  // MIDI is played by one instrument: at first the built-in one
  // (BuiltInSynth), or a SoundFont once loaded. Loading reads the whole
  // SoundFont on this thread; the instrument changes once it's ready, and
  // an error leaves the one playing.
  LoadResult loadSoundFont(const std::string &path);
  LoadResult loadSoundFont(const void *data, std::size_t size);
  void useBuiltInInstrument();

  // Plays a MIDI event on the instrument now: a key pressed on a keyboard,
  // say
  void midi(const MidiEvent &event);

  // Plays a MIDI file, or a sequence, on the instrument, in the Music
  // group. One plays at a time: starting one stops the last. `settings`:
  // volume and pan as for any voice, pitch is speed, and loop. Returns its
  // voice, or nothing (with the reason in `error`).
  std::optional<VoiceId> playMidi(const std::string &path, const VoiceSettings &settings,
                                  std::string &error);
  std::optional<VoiceId> playMidi(std::shared_ptr<const MidiSequence> sequence,
                                  const VoiceSettings &settings, std::string &error);

  // Volumes: one per group, under a master volume (1 as recorded)
  void setGroupVolume(Group group, float volume);
  void setMasterVolume(float volume);

  // Call often (every few tens of milliseconds): decodes music ahead, sends
  // commands that didn't fit the queue, and collects what the audio thread
  // reported. Returns the voices that finished since the last call.
  std::vector<VoiceId> update();

  bool isPlaying(VoiceId voice) const { return voices_.contains(voice); }
  std::size_t voicesPlaying() const { return voices_.size(); }
  const MixerStats &stats() const { return mixer_.stats(); }

private:
  struct Music; // a decoder and its ring (Engine.cpp)
  struct VoiceInfo {
    std::shared_ptr<Music> music;                 // set for streamed music
    std::shared_ptr<const MidiSequence> sequence; // set for MIDI
    float volume = 1.0f;                          // as last set, for choosing one to take over
    bool stopping = false;                        // fading out: no longer counted against maxVoices
  };

  // Voices playing and not stopping: what maxVoices limits
  std::size_t liveVoices() const;
  // Makes room for a voice at `volume` (see WhenVoicesFull); false if
  // there's none
  bool makeRoom(float volume);

  void send(const Command &command);
  void flushPending();
  void collectEvents(std::vector<VoiceId> &finished);
  void decodeAhead(Music &music);
  LoadResult store(const std::string &name, std::shared_ptr<Sound> sound);
  void useSynth(std::unique_ptr<Synth> synth);

  Output &output_;
  EngineConfig config_;
  std::uint32_t sampleRate_;
  Mixer mixer_;
  bool started_ = false;
  VoiceId nextVoice_ = 1;
  std::map<std::string, std::shared_ptr<Sound>> sounds_;
  // Sounds unloaded or replaced, kept until the audio thread forgets them
  std::vector<std::shared_ptr<Sound>> retiring_;
  std::map<VoiceId, VoiceInfo> voices_;
  std::deque<Command> pending_;  // commands the queue had no room for yet
  std::unique_ptr<Synth> synth_; // the instrument
  // Instruments replaced, kept until the audio thread forgets them
  std::vector<std::unique_ptr<Synth>> retiringSynths_;
  VoiceId midiVoice_ = 0; // the MIDI sequence playing, if any
};

} // namespace ScratchPadd::Audio
