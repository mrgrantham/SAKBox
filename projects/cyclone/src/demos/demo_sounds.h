/*
 * SAKBox: sound for every Cyclone demo, played with ScratchPadd's audio
 * engine (//projects/scratchpadd/audio).
 */
#pragma once

#include <ScratchPadd/Audio/Engine.hpp>

#include <cstdint>
#include <map>
#include <string>

/**
 * The demos' sounds, made in code (ScratchPadd::Audio::MadeSounds), so
 * there are no sound files: "knock", "blast", "shot", "zap", "whoosh",
 * "crackle" and "chime" to play once, and the loops "engine" and "wind",
 * which a demo starts and then changes as it goes (an engine's pitch with
 * its speed, say). Every Application has one; V turns it off and on.
 */
class DemoSounds {
public:
  /** Plays through `output`; if it can't start, stays silent (see error()) */
  explicit DemoSounds(ScratchPadd::Audio::Output &output);

  /**
   * Plays sound `name` once. False if it didn't play: sound is off or
   * can't play, or it's quieter than every sound playing while every voice
   * is busy (the engine takes over the quietest for a louder one).
   */
  bool play(const std::string &name, const ScratchPadd::Audio::VoiceSettings &settings = {});

  /**
   * Plays loop `name` as `settings` say (looping whatever they say),
   * starting it if it isn't playing: call it as often as the loop should
   * change. While sound is off it's silent, and it comes back as last set.
   */
  void setLoop(const std::string &name, const ScratchPadd::Audio::VoiceSettings &settings);

  /** Stops loop `name`, if it's playing */
  void stopLoop(const std::string &name);

  /** Call every frame: collects finished sounds */
  void update() { engine_.update(); }

  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }
  bool playing() const { return error_.empty(); }
  const std::string &error() const { return error_; }

  /** Sounds played so far (loops once each time they start) */
  std::uint32_t played() const { return played_; }

private:
  struct Loop {
    ScratchPadd::Audio::VoiceId voice = 0;
    ScratchPadd::Audio::VoiceSettings settings; // as last set
  };

  ScratchPadd::Audio::Engine engine_;
  std::map<std::string, Loop> loops_;
  std::string error_;
  bool enabled_ = true;
  std::uint32_t played_ = 0;
};
