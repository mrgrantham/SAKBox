#include <ScratchPadd/ScratchPadd.hpp>

// Worker Padds used in system
#include <AudioPadd.hpp>
#include <DisplayPadd.hpp>
#include <MidiInputPadd.hpp>
#include <ShapeLoggerPadd.hpp>
#include <StoryPadd.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <signal.h> //  our new library
#include <spdlog/spdlog.h>

// backward::SignalHandling sh;
// volatile sig_atomic_t flag = 0;
// void signal_handler(int sig);

ScratchPadd::System *spsystem =
    ScratchPadd::SystemBuilder<DisplayPadd, StoryPadd, AudioPadd, MidiInputPadd, ShapeLoggerPadd>();

int main(int argc, char **argv) {
  DataDepRetriever::ConfigureDependencies(argv[0]);
  SCOPED_METHOD_TIMER();

  // signal(SIGINT, signal_handler);
  spsystem->instantiate();
  spsystem->start();
}

void signal_handler(int sig) { // can be called asynchronously
  spdlog::info("shutting d own");
  // flag = 0; // set flag
  spsystem->stop();
}