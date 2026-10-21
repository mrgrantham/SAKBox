#pragma once

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

struct EngineConfig {
  std::uint32_t maxVoices = 32;       // sounds and music playing at once
  std::uint32_t queueCapacity = 1024; // commands waiting for the audio thread
  double musicAheadSeconds = 0.5;     // music decoded ahead of what's playing
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
    std::shared_ptr<Music> music; // null for a sound
  };

  void send(const Command &command);
  void flushPending();
  void collectEvents(std::vector<VoiceId> &finished);
  void decodeAhead(Music &music);
  LoadResult store(const std::string &name, std::shared_ptr<Sound> sound);

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
  std::deque<Command> pending_; // commands the queue had no room for yet
};

} // namespace ScratchPadd::Audio
