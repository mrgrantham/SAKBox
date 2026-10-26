#pragma once

// AudioPadd plays sounds and music for the other padds, which ask with the
// messages in ScratchPadd/Audio/Messages.hpp (see the README's Audio
// section). It has two implementations under the one name, so the padds
// that use it, and the messages, are the same everywhere; this picks the
// platform's:
//
//   AudioPadd/MixingAudioPadd.hpp            desktops and the 3DS: mixes in
//                                            software with the audio engine
//   AudioPadd/HardwareChannelsAudioPadd.hpp  the Nintendo DS: plays each sound
//                                            on one of its 16 hardware sound
//                                            channels (mixing in software is
//                                            too slow there)
#if defined(__NDS__)
#include <AudioPadd/HardwareChannelsAudioPadd.hpp>
#else
#include <AudioPadd/MixingAudioPadd.hpp>
#endif
