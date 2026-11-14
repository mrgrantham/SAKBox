#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>

// Worker Padds used in system
#include <AudioPadd.hpp>
#include <DisplayPadd.hpp>
#include <MidiInputPadd.hpp>
#include <ShapeLoggerPadd.hpp>
#include <StoryPadd.hpp>

#include <chrono>
#include <iostream>
#include <signal.h> //  our new library
#include <spdlog/spdlog.h>

// backward::SignalHandling sh;
// volatile sig_atomic_t flag = 0;
// void signal_handler(int sig);

// (global, for signal_handler)
const std::unique_ptr<ScratchPadd::System> kSystem =
    ScratchPadd::SystemBuilder<ScratchPadd::AboutPadd, DisplayPadd, StoryPadd, AudioPadd,
                               MidiInputPadd, ShapeLoggerPadd, ScratchPadd::Storage::StoragePadd>();

int main(int argc, char **argv) {
  DataDepRetriever::ConfigureDependencies(argv[0]);
  // The log, in the terminal and a file (StoragePadd writes it when idle)
  ScratchPadd::DebugLog::Session log;
  SCOPED_METHOD_TIMER();

  // signal(SIGINT, signal_handler);
  kSystem->instantiate();
  kSystem->start();
}

void signal_handler(int sig) { // can be called asynchronously
  spdlog::info("shutting d own");
  // flag = 0; // set flag
  kSystem->stop();
}