#include <ScratchPadd/DataDependencies.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

// Worker Padds used in system
#include <AudioPadd.hpp>
#include <DisplayPadd.hpp>
#include <StoryPadd.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <signal.h> //  our new library
#include <spdlog/spdlog.h>

// backward::SignalHandling sh;
// volatile sig_atomic_t flag = 0;
// void signal_handler(int sig);

#if !defined(SCRATCHPADD_TESTS)
#define SCRATCHPADD_GL4 DisplayPadd, StoryPadd, AudioPadd
ScratchPadd::System *spsystem = ScratchPadd::SystemBuilder<SCRATCHPADD_GL4>();
#else
#define SCRATCHPADD_TEST_PADDS DisplayPadd, StoryPadd, AudioPadd
ScratchPadd::System *spsystem =
    ScratchPadd::SystemBuilder<SCRATCHPADD_TEST_PADDS>();
#endif

int main(int argc, char **argv) {
  ScratchPadd::Data::ConfigureDependencies(argv[0]);
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