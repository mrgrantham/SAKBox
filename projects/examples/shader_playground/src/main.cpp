#include <spdlog/spdlog.h>
#include <stdlib.h>
#include "backward/backward.hpp"

#include <ScratchPadd/Graphics/Graphics.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <DataDepRetriever/DataDependencies.hpp>

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
    graphics_->setupWindow(1600, 1000);
    PADDLOG_INFO("setup windows");

    const std::string shaderFolder =
        "/projects/examples/shader_playground/shaders/";

    SampleItems::Circle circle(0.0, 0.0, 0.8, 100);
    auto view1 =
        GetGL_ViewBuilder()
            .setName("GL Example View 1")
            .setFrameBuffer(800, 600)
            .setSize(300, 300)
            .setPosition(20, 40)
            .setFragmentShaderPath(GetFragmentShaderPath("play2", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("play2", shaderFolder))
            .setGeometry(circle.vertices(), circle.indices())
            .build();
    graphics_->addView(std::move(view1));

    auto view2 =
        GetGL_ViewBuilder()
            .setName("GL Example View 2")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(20, 350)
            .setFragmentShaderPath(GetFragmentShaderPath("play", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("play", shaderFolder))
            .setGeometry(SampleItems::vertices, SampleItems::indices)
            .build();
    graphics_->addView(std::move(view2));

    auto view3 =
        GetGL_ViewBuilder()
            .setName("GL Example View 3")
            .setFrameBuffer(800, 600)
            .setSize(300, 300)
            .setPosition(400, 40)
            .setFragmentShaderPath(GetFragmentShaderPath("play3", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("play3", shaderFolder))
            .setGeometry(SampleItems::Square::vertices,
                         SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(view3));

    auto view4 =
        GetGL_ViewBuilder()
            .setName("GL Example View 4")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(800, 40)
            .setFragmentShaderPath(GetFragmentShaderPath("play4", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("play4", shaderFolder))
            .setGeometry(SampleItems::Square::vertices,
                         SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(view4));

    auto view5 =
        GetGL_ViewBuilder()
            .setName("GL Example View 5")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(1200, 40)
            .setFragmentShaderPath(GetFragmentShaderPath("play5", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices,
                         SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(view5));

    auto view6 =
        GetGL_ViewBuilder()
            .setName("GL Example View 6")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(400, 350)
            .setFragmentShaderPath(GetFragmentShaderPath("play6", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::vertices, SampleItems::indices)
            .build();
    graphics_->addView(std::move(view6));

    auto view7 =
        GetGL_ViewBuilder()
            .setName("GL Example View 7")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(800, 350)
            .setFragmentShaderPath(GetFragmentShaderPath("play7", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices,
                         SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(view7));

    auto view8 =
        GetGL_ViewBuilder()
            .setName("GL Example View 8")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(1200, 350)
            .setFragmentShaderPath(GetFragmentShaderPath("play8", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices,
                         SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(view8));

    auto view9 =
        GetGL_ViewBuilder()
            .setName("GL Example View 9")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(20, 670)
            .setFragmentShaderPath(GetFragmentShaderPath("play9", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices,
                         SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(view9));
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
  // Makes data deps declared in BUILD file retrievable. Will hang
  // when GetFullDependencyPath is called if this is not called first
  DataDepRetriever::ConfigureDependencies(argv[0]);
  spdlog::info("Signal handling");
  backward::SignalHandling sh;
  ScratchPadd::System *spsystem = ScratchPadd::SystemBuilder<Playground>();
  spdlog::info("Testing");
  spsystem->instantiate();
  spsystem->start();

  return EXIT_SUCCESS;
}