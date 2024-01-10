#include <spdlog/spdlog.h>
#include <stdlib.h>

#include "backward/backward.hpp"
#include <ScratchPadd/Graphics/Graphics.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

using namespace std::chrono_literals;

class Playground : public ScratchPadd::Base {
private:
  std::unique_ptr<Graphics> graphics_;
  ScratchPadd::Timer performanceTimer_;

public:
  bool shouldUseMainThread() override { return true; }

  Playground(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }

  void prepare() override {
    PADDLOG_INFO("Preparing: {}", name());
    // We dont want the work loop to sleep
    // TODO make this sleep/wake from semaphore
    work_thread_sleep_interval_ = 0ms;
    graphics_ = GraphicsBuilder();
    performanceTimer_.setTimerName(name());
    performanceTimer_.start();
  }

  void starting() override {
    PADDLOG_INFO("setting up windows");
    graphics_->setupWindow(1400, 1000);
    PADDLOG_INFO("setup windows");

    const std::string shaderFolder =
        "/projects/examples/opengl_playground/shaders/";

    auto view1 =
        GetGL_ViewBuilder()
            .setName("GL Example View 1")
            .setFrameBuffer(800, 600)
            .setSize(300, 300)
            .setPosition(20, 40)
            .setFragmentShaderPath(
                getShaderPath("fragment_play2", shaderFolder))
            .setVertexShaderPath(getShaderPath("vertex_play2", shaderFolder))
            .build();
    PADDLOG_INFO("created view 1");
    graphics_->addView(std::move(view1));
    PADDLOG_INFO("added view 1");

    auto view2 =
        GetGL_ViewBuilder()
            .setName("GL Example View 2")
            .setFrameBuffer(400, 500)
            .setSize(200, 200)
            .setPosition(20, 500)
            .setFragmentShaderPath(getShaderPath("fragment_play", shaderFolder))
            .setVertexShaderPath(getShaderPath("vertex_play", shaderFolder))
            .build();
    PADDLOG_INFO("created view 2");

    graphics_->addView(std::move(view2));
    PADDLOG_INFO("added view 2");

    PADDLOG_INFO("Added views");
  }

  void finishing() override {
    PADDLOG_INFO("[{}] Avg Repeating Interval: {}", name(),
                 performanceTimer_.getAverageIntervalString());
    performanceTimer_.markTimeAndPrint();
    graphics_->tearDown();
  }

  std::string name() override { return __CLASS_NAME__; }

  void receive(ScratchPadd::Message message) override {}

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {};
  }

  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return 16ms;
  }

  void repeat() override {
    performanceTimer_.markInterval();
    PADDLOG_INTERVAL(spdlog::level::info, 1000ms,
                     "DisplayPadd drawing at {:.3f}s interval",
                     performanceTimer_.getAverageIntervalInSeconds(5));
    PADDLOG_INTERVAL(spdlog::level::info, 1000ms,
                     "DisplayPadd drawing at {:.3f} fps",
                     1.0 / performanceTimer_.getAverageIntervalInSeconds(5));

    if (!graphics_->draw()) {
      PADDLOG_INFO("Stop drawing");
      system_->stop();
    }
  }
};

int main(int argc, char **argv) {
  // Makes data deps declared in BUILD file retrievable. Will hang otherwise
  // when GetFullDependencyPath is called
  ScratchPadd::Data::ConfigureDependencies(argv[0]);
  spdlog::info("Signal handling");
  backward::SignalHandling sh;
  ScratchPadd::System *spsystem = ScratchPadd::SystemBuilder<Playground>();
  spdlog::info("Testing");
  spsystem->instantiate();
  spsystem->start();

  return EXIT_SUCCESS;
}