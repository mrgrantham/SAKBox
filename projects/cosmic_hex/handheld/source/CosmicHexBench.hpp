// CosmicHexBench.hpp - the -bench build's timings (COSMIC_HEX_BENCH; round
// 92). melonDS showed the DS's bots playing 24 to 27 simulated games a move,
// a DS 5: emulators don't model the caches (the BlocksDS optimisation
// guide: "measure CPU performance on hardware only"). So this times the
// bots' parts on their own, before the app starts (nothing else running),
// and shows a table to read off the screen, the same in melonDS and on a
// console, to compare line by line.
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>
#include <CosmicHex/Rules.hpp>
#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace CosmicHexBench {

using Clock = std::chrono::steady_clock;
using CosmicHex::Game;
using CosmicHex::State;

// Microseconds since `start`, as a whole number (no floating point)
inline long long micros(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
}

// One random action for whoever's turn it is (the dealer's draw for the
// dealer), applied; and the game started again once it's over
inline void step(State &state, SeededRandom::Generator &random, const State &restart) {
  if (Game::isOver(state)) {
    state = restart;
    return;
  }
  if (Game::currentPlayer(state) == GameBots::kDealer) {
    Game::apply(state, Game::dealerAction(state, random));
    return;
  }
  const std::vector<Game::Action> legal = Game::legalActions(state);
  Game::apply(state, legal[random.below(static_cast<std::uint32_t>(legal.size()))]);
}

// The timings, printed (to the console the caller set up)
inline void run() {
  std::printf("Cosmic Hex bench\n\n");
  // A clock read
  {
    const Clock::time_point start = Clock::now();
    long long sink = 0;
    for (int i = 0; i < 1000; ++i)
      sink += Clock::now().time_since_epoch().count() & 1;
    std::printf("clock read    %lld ns  (%lld)\n", micros(start), sink & 1);
  }
  // Copying plain memory, a block at a time (how big a State that copies
  // without allocating can afford to be)
  for (const std::size_t bytes : {std::size_t{4096}, std::size_t{16384}}) {
    std::vector<unsigned char> from(bytes, 1), to(bytes, 0);
    const Clock::time_point start = Clock::now();
    for (int i = 0; i < 100; ++i) {
      std::memcpy(to.data(), from.data(), bytes);
      from[static_cast<std::size_t>(i)] = to[bytes - 1]; // (so it isn't optimised away)
    }
    std::printf("copy %2zu KB    %lld us\n", bytes / 1024, micros(start) / 100);
  }
  // A game partway: the setup and some turns, played at random
  SeededRandom::Generator random(92);
  const State fresh = CosmicHex::newGame(4, CosmicHex::agreedRules(CosmicHex::boardR3()));
  State middle = fresh;
  for (int i = 0; i < 120; ++i)
    step(middle, random, fresh);
  // A copy of the state (each simulated game starts with one)
  {
    const Clock::time_point start = Clock::now();
    long long sink = 0;
    for (int i = 0; i < 200; ++i) {
      State copy = middle;
      sink += copy.round;
    }
    std::printf("state copy    %lld us  (%lld)\n", micros(start) / 200, sink & 1);
  }
  // Listing the legal moves, on its own
  {
    const Clock::time_point start = Clock::now();
    std::size_t sink = 0;
    for (int i = 0; i < 200; ++i)
      sink += Game::legalActions(middle).size();
    std::printf("list moves    %lld us  (%zu)\n", micros(start) / 200, sink & 1);
  }
  // Rollout actions: list and apply, as a simulated game does
  {
    State state = middle;
    const Clock::time_point start = Clock::now();
    for (int i = 0; i < 1000; ++i)
      step(state, random, middle);
    std::printf("rollout step  %lld us\n", micros(start) / 1000);
  }
  // Drawing a picture from its recipe, as the DS does (Coarse), at the
  // size it's drawn now and twice that: a tile face (a planet) and an
  // action card, for close-ups drawn when they're shown
  for (const int side : {32, 64}) {
    const Clock::time_point start = Clock::now();
    const auto face = CosmicHex::Handheld::recipePixels(
        CosmicHex::Art::recipeOf(CosmicHex::Art::Face::PlanetFuel), side, side,
        ScratchPadd::TextureRecipe::Smoothing::Coarse);
    const long long faceUs = micros(start);
    const Clock::time_point cardStart = Clock::now();
    const auto card = CosmicHex::Handheld::recipePixels(
        CosmicHex::Art::cardRecipeOf(CosmicHex::Art::Card::InterstellarCartel), side,
        side * 88 / 63, ScratchPadd::TextureRecipe::Smoothing::Coarse);
    std::printf("face %d %lld ms card %lld ms (%zu)\n", side, faceUs / 1000,
                micros(cardStart) / 1000, (face.size() + card.size()) & 1);
  }
  // The search: 20 simulated games from the middle, without and with its
  // per-phase timing (the -watch builds time each phase)
  for (const bool timed : {false, true}) {
    if (CosmicHex::Game::currentPlayer(middle) == GameBots::kDealer)
      Game::apply(middle, Game::dealerAction(middle, random));
    GameBots::Ismcts<Game> bot({.rolloutDepth = 60});
    GameBots::SearchTimes times;
    GameBots::Budget budget{.iterations = 20, .frameArenaBytes = 64 * 1024};
    if (timed)
      budget.times = &times;
    const Clock::time_point start = Clock::now();
    (void)bot.choose(middle, random, budget);
    std::printf("game, %s %lld ms\n", timed ? "timed  " : "untimed", micros(start) / 20 / 1000);
  }
  std::printf("\nDone: note these, then\nswitch off.\n");
}

} // namespace CosmicHexBench
