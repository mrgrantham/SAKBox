#include "fmt/core.h"
#include "hello.hpp"

int main(int argc, char **argv) {
  fmt::print("testing this repo: {}\n", makeGreeting());
  return 0;
}