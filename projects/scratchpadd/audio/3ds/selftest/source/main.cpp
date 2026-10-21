// The ScratchPadd audio core's checks, run on a 3DS: the same engine the
// desktop uses, built by devkitARM and mixing offline (no sound plays), so
// each check reads the exact samples. Then a full mix is timed on the 3DS's
// CPU. Results show on the top screen and are written to
// sdmc:/scratchpadd-audio-selftest.txt; START exits.
#include <3ds.h>

#include <ScratchPadd/Audio/Engine.hpp>
#include <miniaudio/miniaudio.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;

namespace {

// The rate of the 3DS's DSP (SYSCLOCK_SOC / 512), which the 3DS output will
// mix at
constexpr std::uint32_t kRate = 32728;

int passed = 0;
int failed = 0;
FILE *results = nullptr; // the results file on the SD card, if it opened

// Prints a line on screen in `colour` (an ANSI code, or "" for none) and
// writes it, without colour, to the results file
void report(const char *colour, const char *format, ...) {
  std::va_list args;
  va_start(args, format);
  char line[128];
  std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  std::printf("%s%s\x1b[0m\n", colour, line);
  if (results) {
    std::fprintf(results, "%s\n", line);
  }
}

void check(bool ok, const char *what) {
  report(ok ? "\x1b[32m" : "\x1b[31m", "%s %s", ok ? "PASS" : "FAIL", what);
  ++(ok ? passed : failed);
}

bool near(float a, float b, float tolerance = 1e-5f) { return std::fabs(a - b) <= tolerance; }

// An engine playing into an offline output, rendered on demand
struct Offline {
  OfflineOutput output{kRate};
  Engine engine{output};
  Offline() {
    std::string error;
    engine.start(error);
  }
  std::vector<float> render(std::uint32_t frames) {
    std::vector<float> out(2 * frames);
    output.render(out.data(), frames);
    return out;
  }
  bool load(const char *name, const std::vector<float> &stereo) {
    return engine.loadSamples(name, stereo.data(), stereo.size(), kRate, 2).ok;
  }
};

// `frames` stereo frames: left counts up from 0.01, right is minus that
std::vector<float> ramp(std::uint32_t frames) {
  std::vector<float> stereo;
  for (std::uint32_t i = 0; i < frames; ++i) {
    stereo.push_back(0.01f * static_cast<float>(i + 1));
    stereo.push_back(-0.01f * static_cast<float>(i + 1));
  }
  return stereo;
}

void playsAsLoaded() {
  Offline offline;
  offline.load("ramp", ramp(100));
  offline.engine.play("ramp");
  const std::vector<float> out = offline.render(128);
  const std::vector<float> expected = ramp(100);
  bool same = true;
  for (std::size_t i = 0; i < out.size(); ++i)
    same = same && out[i] == (i < expected.size() ? expected[i] : 0.0f);
  check(same, "a sound plays as loaded");
}

void volumesAndPan() {
  Offline offline;
  offline.load("one", std::vector<float>(128, 1.0f));
  offline.engine.setMasterVolume(0.5f);
  offline.engine.setGroupVolume(Group::Sounds, 0.5f);
  offline.engine.play("one", {.volume = 2.0f, .pan = 0.5f});
  const std::vector<float> out = offline.render(16);
  // 2 x 0.5 x 0.5 = 0.5; panned right by half, so the left is halved
  check(near(out[0], 0.25f) && near(out[1], 0.5f), "volumes multiply; pan balances");
}

void pitch() {
  Offline slow;
  slow.load("ramp", ramp(100));
  slow.engine.play("ramp", {.pitch = 0.5f});
  const std::vector<float> halfSpeed = slow.render(4);
  check(near(halfSpeed[2], 0.015f) && near(halfSpeed[4], 0.02f), "half speed interpolates");
  Offline fast;
  fast.load("ramp", ramp(100));
  fast.engine.play("ramp", {.pitch = 2.0f});
  const std::vector<float> out = fast.render(64);
  check(near(out[2], 0.03f) && out[2 * 50] == 0.0f, "double speed: half as long");
}

void loopAndStop() {
  Offline offline;
  offline.load("ramp", ramp(10));
  const auto voice = offline.engine.play("ramp", {.loop = true});
  const std::vector<float> looped = offline.render(25);
  offline.engine.stop(*voice);
  const std::vector<float> fading = offline.render(100);
  const std::vector<VoiceId> finished = offline.engine.update();
  check(near(looped[2 * 10], 0.01f) && near(looped[2 * 24], 0.05f), "a loop goes round");
  check(std::fabs(fading[2 * 99]) < 0.001f && finished.size() == 1 && finished[0] == *voice,
        "stopping fades out, then finishes");
}

void resamples() {
  // 0.1 s of a 440 Hz sine at 22,050 Hz, mono, as a game's sound might be
  std::vector<float> mono(2205);
  for (std::size_t i = 0; i < mono.size(); ++i)
    mono[i] = 0.5f * std::sin(6.2831853f * 440.0f * static_cast<float>(i) / 22050.0f);
  Offline offline;
  const LoadResult loaded = offline.engine.loadSamples("tone", mono.data(), mono.size(), 22050, 1);
  offline.engine.play("tone");
  const std::vector<float> out = offline.render(kRate / 5);
  double power = 0;
  std::uint32_t sounding = 0;
  for (std::uint32_t i = 0; i < kRate / 5; ++i) {
    power += out[2 * i] * out[2 * i];
    sounding += out[2 * i] != 0.0f;
  }
  const float rms = static_cast<float>(std::sqrt(power / (kRate / 10)));
  check(loaded.ok && near(rms, 0.3536f, 0.01f) && std::abs(int(sounding) - int(kRate / 10)) < 40,
        "22,050 Hz mono is resampled");
}

// Writes `frames` stereo frames whose left side counts 0..999 thousandths
bool writeCountingWav(const char *path, std::uint32_t frames) {
  ma_encoder_config config =
      ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, kRate);
  ma_encoder encoder;
  if (ma_encoder_init_file(path, &config, &encoder) != MA_SUCCESS)
    return false;
  std::vector<float> data;
  for (std::uint32_t i = 0; i < frames; ++i) {
    data.push_back(static_cast<float>(i % 1000) / 1000.0f);
    data.push_back(0.0f);
  }
  ma_uint64 written = 0;
  ma_encoder_write_pcm_frames(&encoder, data.data(), frames, &written);
  ma_encoder_uninit(&encoder);
  return written == frames;
}

void streamsMusic() {
  const char *path = "sdmc:/scratchpadd-audio-selftest.wav";
  const bool written = writeCountingWav(path, 2500);
  Offline offline;
  std::string error;
  const auto voice = offline.engine.playMusic(path, {.loop = true}, error);
  bool exact = voice.has_value();
  for (std::uint32_t frame = 0; exact && frame < 3 * 2500; frame += 500) {
    const std::vector<float> out = offline.render(500);
    for (std::uint32_t i = 0; i < 500; ++i)
      exact = exact && near(out[2 * i], static_cast<float>((frame + i) % 2500 % 1000) / 1000.0f);
    offline.engine.update();
  }
  check(written && exact && offline.engine.stats().underruns.load() == 0,
        "music streams from SD, looping");
  remove(path);
  check(!offline.engine.playMusic("sdmc:/missing.wav", {}, error) &&
            error.find("can't open") != std::string::npos,
        "a missing file is an error");
}

// 16 voices, pitched (so interpolated) and looping: a second of audio,
// timed on this CPU
void timeAFullMix() {
  Offline offline;
  offline.load("ramp", ramp(4000));
  for (int i = 0; i < 16; ++i)
    offline.engine.play("ramp", {.pitch = 1.3f, .loop = true});
  std::vector<float> out(2 * 512);
  const u64 start = svcGetSystemTick();
  for (std::uint32_t frame = 0; frame < kRate; frame += 512)
    offline.output.render(out.data(), 512);
  const double ms = static_cast<double>(svcGetSystemTick() - start) * 1000.0 / SYSCLOCK_ARM11;
  report("", "16 voices, 1 s at %lu Hz: %.1f ms", static_cast<unsigned long>(kRate), ms);
  check(ms < 500.0, "mixes in under half real time");
}

} // namespace

int main() {
  gfxInitDefault();
  consoleInit(GFX_TOP, nullptr);
  results = std::fopen("sdmc:/scratchpadd-audio-selftest.txt", "w");
  report("\x1b[1m", "ScratchPadd audio core, on a 3DS");
  report("", "Mixing offline at %lu Hz\n", static_cast<unsigned long>(kRate));

  playsAsLoaded();
  volumesAndPan();
  pitch();
  loopAndStop();
  resamples();
  streamsMusic();
  timeAFullMix();

  report(failed ? "\x1b[31m" : "\x1b[32m", "\n%d passed, %d failed", passed, failed);
  if (results) {
    std::fclose(results);
  }
  std::printf("\nPress START to exit.\n");
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START)
      break;
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
  }
  gfxExit();
  return 0;
}
