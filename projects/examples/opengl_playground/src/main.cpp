#include <spdlog/spdlog.h>
#include <stdlib.h>

#include <ScratchPadd/ScratchPadd.hpp>

class Playground : public ScratchPadd::Base {
public:
  bool shouldUseMainThread() override { return true; }

  Playground(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }

  std::string name() override { return __CLASS_NAME__; }

  void receive(ScratchPadd::Message message) override {}

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {};
  }

  std::optional<std::chrono::duration<double>> repeatInterval() override {
    using namespace std::chrono_literals;
    return 500ms;
  }

  void repeat() override {logger().info("repeating: {}",name());}
};

int main() {
  ScratchPadd::System *spsystem = ScratchPadd::SystemBuilder<Playground>();
  spdlog::info("Testing");
  spsystem->instantiate();
  spsystem->start();

  return EXIT_SUCCESS;
}