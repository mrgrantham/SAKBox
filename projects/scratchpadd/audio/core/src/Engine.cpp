#include <ScratchPadd/Audio/Engine.hpp>

#include <ScratchPadd/Audio/BuiltInSynth.hpp>
#include <ScratchPadd/Audio/SoundFontSynth.hpp>

#include <miniaudio/miniaudio.h>

#include <algorithm>

namespace ScratchPadd::Audio {

namespace {
// miniaudio's result as text
std::string describe(ma_result result) { return ma_result_description(result); }

// A decoder that turns any file miniaudio reads into stereo floats at `rate`
ma_decoder_config stereoAt(std::uint32_t rate) {
  return ma_decoder_config_init(ma_format_f32, 2, rate);
}
} // namespace

// Music being streamed: the decoder (read on the owner's thread) and the
// ring it fills for the audio thread
struct Engine::Music {
  ma_decoder decoder{};
  bool open = false;
  bool loop = false;
  StreamBuffer buffer;
  explicit Music(std::uint32_t samples) : buffer(samples) {}
  ~Music() {
    if (open) {
      ma_decoder_uninit(&decoder);
    }
  }
};

Engine::Engine(Output &output, const EngineConfig &config)
    : output_(output), config_(config), sampleRate_(output.sampleRate()),
      mixer_(config.maxVoices, config.queueCapacity) {
  useBuiltInInstrument();
}

Engine::~Engine() { stop(); }

bool Engine::start(std::string &error) {
  if (started_) {
    return true;
  }
  started_ = output_.start(mixer_, error);
  return started_;
}

void Engine::stop() {
  if (started_) {
    output_.stop();
    started_ = false;
  }
}

LoadResult Engine::loadFile(const std::string &name, const std::string &path) {
  ma_decoder decoder;
  const ma_decoder_config config = stereoAt(sampleRate_);
  const ma_result opened = ma_decoder_init_file(path.c_str(), &config, &decoder);
  if (opened != MA_SUCCESS) {
    return {.ok = false, .error = "can't open " + path + ": " + describe(opened)};
  }
  // Read until the decoder runs out: some formats (MP3) only estimate their
  // length up front
  auto sound = std::make_shared<Sound>();
  sound->name = name;
  constexpr ma_uint64 kChunk = 4096;
  ma_result result = MA_SUCCESS;
  for (;;) {
    const std::size_t at = sound->samples.size();
    sound->samples.resize(at + 2 * kChunk);
    ma_uint64 read = 0;
    result = ma_decoder_read_pcm_frames(&decoder, sound->samples.data() + at, kChunk, &read);
    sound->samples.resize(at + 2 * read);
    if (read < kChunk) {
      break;
    }
  }
  ma_decoder_uninit(&decoder);
  if (result != MA_SUCCESS && result != MA_AT_END) {
    return {.ok = false, .error = "can't decode " + path + ": " + describe(result)};
  }
  sound->samples.shrink_to_fit();
  return store(name, std::move(sound));
}

LoadResult Engine::loadSamples(const std::string &name, const float *samples, std::size_t count,
                               std::uint32_t rate, std::uint32_t channels) {
  if (channels == 0 || rate == 0) {
    return {.ok = false, .error = "samples need a rate and at least one channel"};
  }
  const ma_data_converter_config config =
      ma_data_converter_config_init(ma_format_f32, ma_format_f32, channels, 2, rate, sampleRate_);
  ma_data_converter converter;
  const ma_result made = ma_data_converter_init(&config, nullptr, &converter);
  if (made != MA_SUCCESS) {
    return {.ok = false, .error = "can't convert the samples: " + describe(made)};
  }
  ma_uint64 inFrames = count / channels;
  ma_uint64 outFrames = 0;
  ma_data_converter_get_expected_output_frame_count(&converter, inFrames, &outFrames);
  auto sound = std::make_shared<Sound>();
  sound->name = name;
  sound->samples.resize(2 * outFrames);
  const ma_result converted = ma_data_converter_process_pcm_frames(
      &converter, samples, &inFrames, sound->samples.data(), &outFrames);
  ma_data_converter_uninit(&converter, nullptr);
  if (converted != MA_SUCCESS) {
    return {.ok = false, .error = "can't convert the samples: " + describe(converted)};
  }
  sound->samples.resize(2 * outFrames);
  return store(name, std::move(sound));
}

LoadResult Engine::store(const std::string &name, std::shared_ptr<Sound> sound) {
  auto &slot = sounds_[name];
  if (slot) {
    unload(name);
    sounds_[name] = std::move(sound);
  } else {
    slot = std::move(sound);
  }
  return {.ok = true};
}

bool Engine::unload(const std::string &name) {
  auto found = sounds_.find(name);
  if (found == sounds_.end()) {
    return false;
  }
  // Keep it until the audio thread has stopped every voice reading it
  send({.kind = Command::Kind::Forget, .sound = found->second.get()});
  retiring_.push_back(std::move(found->second));
  sounds_.erase(found);
  return true;
}

std::optional<VoiceId> Engine::play(const std::string &name, const VoiceSettings &settings) {
  auto found = sounds_.find(name);
  if (found == sounds_.end() || voices_.size() >= config_.maxVoices) {
    return std::nullopt;
  }
  const VoiceId voice = nextVoice_++;
  voices_[voice] = {};
  send({.kind = Command::Kind::Play,
        .voice = voice,
        .sound = found->second.get(),
        .settings = settings,
        .group = Group::Sounds});
  return voice;
}

std::optional<VoiceId> Engine::playMusic(const std::string &path, const VoiceSettings &settings,
                                         std::string &error) {
  if (voices_.size() >= config_.maxVoices) {
    error = "every voice is in use";
    return std::nullopt;
  }
  const auto ahead = static_cast<std::uint32_t>(config_.musicAheadSeconds * sampleRate_);
  auto music = std::make_shared<Music>(2 * std::max<std::uint32_t>(ahead, Mixer::kMaxBlock));
  const ma_decoder_config config = stereoAt(sampleRate_);
  const ma_result opened = ma_decoder_init_file(path.c_str(), &config, &music->decoder);
  if (opened != MA_SUCCESS) {
    error = "can't open " + path + ": " + describe(opened);
    return std::nullopt;
  }
  music->open = true;
  music->loop = settings.loop;
  decodeAhead(*music); // so it starts at once
  const VoiceId voice = nextVoice_++;
  send({.kind = Command::Kind::PlayStream,
        .voice = voice,
        .stream = &music->buffer,
        .settings = settings,
        .group = Group::Music});
  voices_[voice] = {.music = std::move(music)};
  return voice;
}

bool Engine::stop(VoiceId voice) {
  if (!voices_.contains(voice)) {
    return false;
  }
  send({.kind = Command::Kind::Stop, .voice = voice});
  return true;
}

bool Engine::set(VoiceId voice, const VoiceSettings &settings) {
  if (!voices_.contains(voice)) {
    return false;
  }
  send({.kind = Command::Kind::Set, .voice = voice, .settings = settings});
  return true;
}

void Engine::stopAll() { send({.kind = Command::Kind::StopAll}); }

LoadResult Engine::loadSoundFont(const std::string &path) {
  std::string error;
  auto synth = SoundFontSynth::load(path, sampleRate_, config_.instrumentNotes, error);
  if (!synth) {
    return {.ok = false, .error = error};
  }
  useSynth(std::move(synth));
  return {.ok = true};
}

LoadResult Engine::loadSoundFont(const void *data, std::size_t size) {
  std::string error;
  auto synth = SoundFontSynth::load(data, size, sampleRate_, config_.instrumentNotes, error);
  if (!synth) {
    return {.ok = false, .error = error};
  }
  useSynth(std::move(synth));
  return {.ok = true};
}

void Engine::useBuiltInInstrument() {
  useSynth(std::make_unique<BuiltInSynth>(sampleRate_, config_.instrumentNotes));
}

void Engine::useSynth(std::unique_ptr<Synth> synth) {
  send({.kind = Command::Kind::SetSynth, .synth = synth.get()});
  if (synth_) {
    retiringSynths_.push_back(std::move(synth_));
  }
  synth_ = std::move(synth);
}

void Engine::midi(const MidiEvent &event) { send({.kind = Command::Kind::Midi, .midi = event}); }

std::optional<VoiceId> Engine::playMidi(const std::string &path, const VoiceSettings &settings,
                                        std::string &error) {
  auto sequence = readMidiFile(path, sampleRate_, error);
  return sequence ? playMidi(std::move(sequence), settings, error) : std::nullopt;
}

std::optional<VoiceId> Engine::playMidi(std::shared_ptr<const MidiSequence> sequence,
                                        const VoiceSettings &settings, std::string &error) {
  stop(midiVoice_);
  if (voices_.size() >= config_.maxVoices) {
    error = "every voice is in use";
    return std::nullopt;
  }
  const VoiceId voice = nextVoice_++;
  send({.kind = Command::Kind::PlaySequence,
        .voice = voice,
        .sequence = sequence.get(),
        .settings = settings,
        .group = Group::Music});
  voices_[voice] = {.sequence = std::move(sequence)};
  midiVoice_ = voice;
  return voice;
}

void Engine::setGroupVolume(Group group, float volume) {
  send({.kind = Command::Kind::SetGain, .group = group, .gain = volume});
}

void Engine::setMasterVolume(float volume) {
  send({.kind = Command::Kind::SetGain, .master = true, .gain = volume});
}

std::vector<VoiceId> Engine::update() {
  flushPending();
  for (auto &[voice, info] : voices_) {
    if (info.music) {
      decodeAhead(*info.music);
    }
  }
  std::vector<VoiceId> finished;
  collectEvents(finished);
  return finished;
}

void Engine::send(const Command &command) {
  // In order: nothing jumps ahead of commands still waiting
  if (!pending_.empty() || !mixer_.commands().push(command)) {
    pending_.push_back(command);
  }
}

void Engine::flushPending() {
  while (!pending_.empty() && mixer_.commands().push(pending_.front())) {
    pending_.pop_front();
  }
}

void Engine::collectEvents(std::vector<VoiceId> &finished) {
  Event event;
  while (mixer_.events().pop(event)) {
    if (event.kind == Event::Kind::Finished) {
      // A stream's buffer is freed here, now the audio thread is done with it
      if (voices_.erase(event.voice) != 0) {
        finished.push_back(event.voice);
      }
    } else if (event.synth) {
      std::erase_if(retiringSynths_, [&](const auto &synth) { return synth.get() == event.synth; });
    } else {
      std::erase_if(retiring_, [&](const auto &sound) { return sound.get() == event.sound; });
    }
  }
}

void Engine::decodeAhead(Music &music) {
  if (music.buffer.ended.load(std::memory_order_relaxed)) {
    return;
  }
  // Top the ring up in chunks, from a stack buffer (no allocation)
  constexpr std::uint32_t kChunk = 1024;
  float chunk[2 * kChunk];
  SampleRing &ring = music.buffer.ring;
  bool readSinceSeek = true; // false right after looping back to the start
  while (ring.capacity() - ring.available() >= 2 * kChunk) {
    ma_uint64 read = 0;
    const ma_result result = ma_decoder_read_pcm_frames(&music.decoder, chunk, kChunk, &read);
    ring.write(chunk, static_cast<std::uint32_t>(2 * read));
    readSinceSeek = readSinceSeek || read > 0;
    if (read == kChunk && result == MA_SUCCESS) {
      continue;
    }
    // At the end: loop back, unless the file gave nothing even from the
    // start (it's empty) or won't seek
    if (music.loop && readSinceSeek &&
        ma_decoder_seek_to_pcm_frame(&music.decoder, 0) == MA_SUCCESS) {
      readSinceSeek = false;
      continue;
    }
    music.buffer.ended.store(true, std::memory_order_release);
    return;
  }
}

} // namespace ScratchPadd::Audio
