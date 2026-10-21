#pragma once

#include <ScratchPadd/Audio/Output.hpp>
#include <ScratchPadd/Audio/SpscQueue.hpp>
#include <ScratchPadd/Audio/Voice.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ScratchPadd::Audio {

// A sound decoded and ready to play: interleaved stereo at the engine's
// sample rate, so the mixer only ever copies or interpolates it
struct Sound {
  std::string name;
  std::vector<float> samples; // left, right, left, right, ...
  std::uint32_t frames() const { return static_cast<std::uint32_t>(samples.size() / 2); }
};

// Music decoded ahead on the owner's thread, played from a ring of samples
// by the audio thread (see Engine). `ended` is set by the owner once the
// decoder has nothing more to add.
struct StreamBuffer {
  explicit StreamBuffer(std::uint32_t samples) : ring(samples) {}
  SampleRing ring;
  std::atomic<bool> ended{false};
};

// What the owner asks the audio thread to do (see Engine)
struct Command {
  enum class Kind : std::uint8_t { Play, PlayStream, Stop, Set, StopAll, Forget, SetGain };
  Kind kind = Kind::StopAll;
  VoiceId voice = 0;
  const Sound *sound = nullptr;
  StreamBuffer *stream = nullptr;
  VoiceSettings settings;
  Group group = Group::Sounds;
  bool master = false; // SetGain: the master volume rather than `group`'s
  float gain = 1.0f;
};

// What the audio thread tells the owner
struct Event {
  enum class Kind : std::uint8_t { Finished, Forgotten };
  Kind kind = Kind::Finished;
  VoiceId voice = 0;            // Finished: which voice
  const Sound *sound = nullptr; // Forgotten: no voice uses it any more
};

// Counters the audio thread keeps, readable from any thread. They wrap
// around after 2^32.
struct MixerStats {
  std::atomic<std::uint32_t> blocks{0};        // render() calls
  std::atomic<std::uint32_t> frames{0};        // frames rendered
  std::atomic<std::uint32_t> underruns{0};     // blocks where music ran dry
  std::atomic<std::uint32_t> droppedEvents{0}; // events the owner's queue had no room for
  std::atomic<std::uint32_t> voicesPlaying{0};
};

// The audio-thread half of the engine: plays voices into the output's
// buffer. Everything it needs is allocated by its constructor; render()
// only reads commands, mixes and writes events, so it never locks,
// allocates or frees. The owner talks to it only through the two queues.
class Mixer final : public Renderer {
public:
  // The longest block mixed at once; longer render() calls are split
  static constexpr std::uint32_t kMaxBlock = 1024;

  Mixer(std::uint32_t maxVoices, std::uint32_t queueCapacity);

  void render(float *interleavedStereo, std::uint32_t frames) noexcept override;

  // The owner pushes commands here and pops events there
  SpscQueue<Command> &commands() { return commands_; }
  SpscQueue<Event> &events() { return events_; }
  const MixerStats &stats() const { return stats_; }

private:
  struct Voice {
    VoiceId id = 0; // 0: free
    const Sound *sound = nullptr;
    StreamBuffer *stream = nullptr;
    VoiceSettings settings;
    Group group = Group::Sounds;
    double position = 0;    // in frames, into `sound`
    float gain[2] = {0, 0}; // left and right, as last mixed
    bool stopping = false;  // fading to silence this block, then freed
  };

  void apply(const Command &command) noexcept;
  void mixBlock(float *out, std::uint32_t frames) noexcept;
  // The left and right gain `voice` should have now
  void targetGain(const Voice &voice, float (&gain)[2]) const noexcept;
  // Mixes `voice` into `out`, ramping from its last gain to `target`;
  // returns false once it has finished
  bool mixSound(Voice &voice, float *out, std::uint32_t frames, const float (&target)[2]) noexcept;
  bool mixStream(Voice &voice, float *out, std::uint32_t frames, const float (&target)[2]) noexcept;
  void finish(Voice &voice) noexcept;
  void report(const Event &event) noexcept;

  std::vector<Voice> voices_;
  std::vector<float> scratch_; // one block of stream samples
  SpscQueue<Command> commands_;
  SpscQueue<Event> events_;
  std::array<float, kGroups> groupGain_{1.0f, 1.0f};
  float masterGain_ = 1.0f;
  MixerStats stats_;
};

} // namespace ScratchPadd::Audio
