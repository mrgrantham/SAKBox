
#include "fmt/core.h"
#include "backward/backward.hpp"
         #include <execinfo.h>
         #include <stdio.h>

void level5() {
  fmt::print("we are deep in this stack!\n");
  backward::Printer printer;

  backward::StackTrace stacktrace;
    stacktrace.load_here();
  printer.print(stacktrace, std::cout);
}

void level4() {
  level5();
}

void level3() {
  level4();
}

void level2() {
  level3();
}

void level1() {
  level2();
}

int main(int argc, char **argv) {
  fmt::print("Lets make a backtrace!\n");
  level1();
  return 0;
}