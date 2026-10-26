// Writes the sounds made in code (MadeSounds.hpp) as raw 16-bit mono
// samples, little-endian, at a rate given, for programs to carry built in
// (see MadeSoundsPcm16.hpp). Run by Bazel when they're built.
//
//   render_made_sounds RATE NAME=PATH...   e.g. 16384 chime=chime.pcm16
#include <ScratchPadd/Audio/MadeSounds.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  using namespace ScratchPadd::Audio;
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s RATE NAME=PATH...\n", argv[0]);
    return 1;
  }
  const auto rate = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10));
  const std::map<std::string, std::function<std::vector<float>(std::uint32_t)>> sounds = {
      {"chime", MadeSounds::chime},
      {"knock", MadeSounds::knock},
      {"blast", MadeSounds::blast},
      {"note", MadeSounds::note}};
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto equals = arg.find('=');
    const auto sound = sounds.find(arg.substr(0, equals));
    if (equals == std::string::npos || sound == sounds.end()) {
      std::fprintf(stderr, "no sound like %s\n", arg.c_str());
      return 1;
    }
    std::ofstream out(arg.substr(equals + 1), std::ios::binary);
    for (const float sample : sound->second(rate)) {
      const auto value =
          static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767));
      const char bytes[2] = {static_cast<char>(value & 0xFF),
                             static_cast<char>((value >> 8) & 0xFF)};
      out.write(bytes, 2);
    }
  }
  return 0;
}
