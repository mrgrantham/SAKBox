// SimulationPadd.hpp - a padd that plays Cosmic Hex games between bots, one
// a message, on its own thread: the balance lab has several, so a batch's
// games are played side by side (one per core), and the lab hands each the
// next game as it reports one done (so stopping a batch is just not handing
// out more).
#pragma once

#include "../../simulate/Batch.hpp"

#include <ScratchPadd/ScratchPadd.hpp>

#include <string>

namespace CosmicHex::Lab {

// Play game `game` of batch `batch`, with these options
struct PlayGame {
  int batch = 0;
  int game = 0;
  Batch::Options options;
};
// What came of it
struct GamePlayed {
  int batch = 0;
  Batch::GameResult result;
};

template <int Id> class SimulationPadd : public ScratchPadd::Base {
public:
  explicit SimulationPadd(ScratchPadd::System *system) : Base(system) {
    receive<PlayGame>([this](const PlayGame &play, ScratchPadd::PaddAddress from) {
      const Batch::Bots bots(play.options);
      reply(from, GamePlayed{play.batch, Batch::playGame(play.options, play.game, bots)});
    });
  }
  std::string name() override { return "Simulation " + std::to_string(Id + 1); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};

} // namespace CosmicHex::Lab
