#include <iostream> 
#include <boost/program_options.hpp>
#include <Workers.hpp>
#include <spdlog/spdlog.h>

#include <signal.h> //  our new library 
#include <ScratchPadd/ScratchPadd.hpp>
#include <chrono>

// backward::SignalHandling sh;
// volatile sig_atomic_t flag = 0;
// void signal_handler(int sig);

#if !defined(SCRATCHPADD_TESTS)
#define SCRATCHPADD_GL4 DisplayPadd,StoryPadd,AudioPadd
ScratchPadd::System *spsystem = ScratchPadd::SystemBuilder<SCRATCHPADD_GL4>();
#else
#define SCRATCHPADD_TEST_PADDS DisplayPadd,StoryPadd,AudioPadd
ScratchPadd::System *spsystem = ScratchPadd::SystemBuilder<SCRATCHPADD_TEST_PADDS>();
#endif

int main(int argc, char **argv) {
  SCOPED_METHOD_TIMER();
  // signal(SIGINT, signal_handler); 
  spdlog::info("Welcome to SCRATCHPADD!");
  spsystem->instantiate();
  spsystem->start();
}

void signal_handler(int sig){ // can be called asynchronously
  spdlog::info("shutting d own");
  // flag = 0; // set flag
  spsystem->stop();
}