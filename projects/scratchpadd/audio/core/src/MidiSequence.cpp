#include <ScratchPadd/Audio/MidiSequence.hpp>

#define TML_IMPLEMENTATION
#include <tinysoundfont/tml.h>

namespace ScratchPadd::Audio {

namespace {
// tml's messages (times in milliseconds) as a sequence at `sampleRate`
std::shared_ptr<const MidiSequence> toSequence(tml_message *first, std::uint32_t sampleRate) {
  auto sequence = std::make_shared<MidiSequence>();
  for (const tml_message *message = first; message; message = message->next) {
    // Tempo, time signature and the like are already in the times
    if (message->type < TML_NOTE_OFF || message->type > TML_PITCH_BEND) {
      continue;
    }
    const auto frame =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(message->time) * sampleRate / 1000);
    MidiEvent event{.status = static_cast<std::uint8_t>(message->type | message->channel)};
    if (message->type == TML_PITCH_BEND) {
      event.data1 = static_cast<std::uint8_t>(message->pitch_bend & 0x7F);
      event.data2 = static_cast<std::uint8_t>((message->pitch_bend >> 7) & 0x7F);
    } else {
      event.data1 = static_cast<std::uint8_t>(message->key & 0x7F);
      event.data2 = static_cast<std::uint8_t>(message->velocity & 0x7F);
    }
    sequence->events.push_back({.frame = frame, .event = event});
    sequence->frames = frame + 1;
  }
  tml_free(first);
  return sequence;
}
} // namespace

std::shared_ptr<const MidiSequence> readMidiFile(const std::string &path, std::uint32_t sampleRate,
                                                 std::string &error) {
  tml_message *first = tml_load_filename(path.c_str());
  if (!first) {
    error = "can't read " + path + " as a MIDI file";
    return nullptr;
  }
  return toSequence(first, sampleRate);
}

std::shared_ptr<const MidiSequence> readMidi(const void *data, std::size_t size,
                                             std::uint32_t sampleRate, std::string &error) {
  tml_message *first = tml_load_memory(data, static_cast<int>(size));
  if (!first) {
    error = "not a MIDI file";
    return nullptr;
  }
  return toSequence(first, sampleRate);
}

} // namespace ScratchPadd::Audio
