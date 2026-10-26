#include "backward/backward.hpp"
#include <spdlog/spdlog.h>
#include <stdlib.h>

#include <DataDepRetriever/DataDependencies.hpp>
#include <ScratchPadd/Graphics/Graphics.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

using namespace std::chrono_literals;

class Playground : public ScratchPadd::Base {
private:
  std::unique_ptr<Graphics> graphics_;
  ScratchPadd::Timer performanceTimer_;

public:
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }

  Playground(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }

  void prepare() override {
    PADDLOG_INFO("Preparing: {}", paddName());
    graphics_ = GraphicsBuilder();
    performanceTimer_.setTimerName(paddName());
    performanceTimer_.start();
  }

  void starting() override {
    PADDLOG_INFO("setting up windows");
    graphics_->setupWindow(1600, 1000);
    PADDLOG_INFO("setup windows");

    const std::string shaderFolder = "/projects/examples/shader_playground/shaders/";

    SampleItems::Circle circle(0.0, 0.0, 0.8, 100);
    auto view1 = GetGL_ViewBuilder()
                     .setName("Blob")
                     .setFrameBuffer(800, 600)
                     .setSize(300, 300)
                     .setPosition(20, 40)
                     .setFragmentShaderPath(GetFragmentShaderPath("blob", shaderFolder))
                     .setVertexShaderPath(GetVertexShaderPath("play2", shaderFolder))
                     .setGeometry(circle.vertices(), circle.indices())
                     .build();
    graphics_->addView(std::move(view1));

    auto view2 = GetGL_ViewBuilder()
                     .setName("GL Example View 2")
                     .setFrameBuffer(300, 300)
                     .setSize(300, 300)
                     .setPosition(20, 350)
                     .setFragmentShaderPath(GetFragmentShaderPath("play", shaderFolder))
                     .setVertexShaderPath(GetVertexShaderPath("play", shaderFolder))
                     .setGeometry(SampleItems::vertices, SampleItems::indices)
                     .build();
    graphics_->addView(std::move(view2));

    auto view3 = GetGL_ViewBuilder()
                     .setName("GL Example View 3")
                     .setFrameBuffer(800, 600)
                     .setSize(300, 300)
                     .setPosition(400, 40)
                     .setFragmentShaderPath(GetFragmentShaderPath("play3", shaderFolder))
                     .setVertexShaderPath(GetVertexShaderPath("play3", shaderFolder))
                     .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
                     .build();
    graphics_->addView(std::move(view3));

    auto viewGradient =
        GetGL_ViewBuilder()
            .setName("Gradient")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(800, 40)
            .setFragmentShaderPath(GetFragmentShaderPath("gradient", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("gradient", shaderFolder))
            .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(viewGradient));

    auto viewDisk = GetGL_ViewBuilder()
                        .setName("Disk")
                        .setFrameBuffer(300, 300)
                        .setSize(300, 300)
                        .setPosition(1200, 40)
                        .setFragmentShaderPath(GetFragmentShaderPath("disk", shaderFolder))
                        .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
                        .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
                        .build();
    graphics_->addView(std::move(viewDisk));

    auto viewParallelagramDisk =
        GetGL_ViewBuilder()
            .setName("Parallelagram Disk")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(400, 350)
            .setFragmentShaderPath(GetFragmentShaderPath("disk", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::vertices, SampleItems::indices)
            .build();
    graphics_->addView(std::move(viewParallelagramDisk));

    auto viewCoolS = GetGL_ViewBuilder()
                         .setName("Cool S")
                         .setFrameBuffer(300, 300)
                         .setSize(300, 300)
                         .setPosition(800, 350)
                         .setFragmentShaderPath(GetFragmentShaderPath("cools", shaderFolder))
                         .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
                         .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
                         .build();
    graphics_->addView(std::move(viewCoolS));

    auto viewCyberFuji2020 =
        GetGL_ViewBuilder()
            .setName("Cyber Fuji 2020")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(1200, 350)
            .setFragmentShaderPath(GetFragmentShaderPath("cyberfuji2020", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(viewCyberFuji2020));

    auto viewShadingArtCodeIntro =
        GetGL_ViewBuilder()
            .setName("Shading Art Code Intro")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(20, 670)
            .setFragmentShaderPath(GetFragmentShaderPath("shadingartcodingintro", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(viewShadingArtCodeIntro));

    auto viewFractalLand =
        GetGL_ViewBuilder()
            .setName("Fractal Land")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(400, 670)
            .setFragmentShaderPath(GetFragmentShaderPath("fractalland", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(viewFractalLand));

    auto viewMandalaFlowers =
        GetGL_ViewBuilder()
            .setName("Mandala Flowers")
            .setFrameBuffer(300, 300)
            .setSize(300, 300)
            .setPosition(800, 670)
            .setFragmentShaderPath(GetFragmentShaderPath("mandalaflowers", shaderFolder))
            .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
            .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
            .build();
    graphics_->addView(std::move(viewMandalaFlowers));

    auto viewFlame = GetGL_ViewBuilder()
                         .setName("Flame")
                         .setFrameBuffer(300, 300)
                         .setSize(300, 300)
                         .setPosition(1200, 670)
                         .setFragmentShaderPath(GetFragmentShaderPath("flame", shaderFolder))
                         .setVertexShaderPath(GetVertexShaderPath("default", shaderFolder))
                         .setGeometry(SampleItems::Square::vertices, SampleItems::Square::indices)
                         .build();
    graphics_->addView(std::move(viewFlame));

    push([this] { drawFrame(); }); // start drawing frames
  }

  void finishing() override {
    PADDLOG_INFO("[{}] Avg Repeating Interval: {}", paddName(),
                 performanceTimer_.getAverageIntervalString());
    performanceTimer_.markTimeAndPrint();
    graphics_->tearDown();
  }

  std::string name() override { return __CLASS_NAME__; }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant> generateControls() override {
    return {};
  }

  // Draws one frame, then queues the next. The buffer swap waits for the
  // display's refresh (vsync), so this runs at the display's rate without a
  // timer and without spinning.
  void drawFrame() {
    performanceTimer_.markInterval();
    PADDLOG_INTERVAL(spdlog::level::info, 1000ms, "DisplayPadd drawing at {:.3f}s interval",
                     performanceTimer_.getAverageIntervalInSeconds(5));
    PADDLOG_INTERVAL(spdlog::level::info, 1000ms, "DisplayPadd drawing at {:.3f} fps",
                     1.0 / performanceTimer_.getAverageIntervalInSeconds(5));

    if (!graphics_->draw()) {
      PADDLOG_INFO("Stop drawing");
      system_->stop();
      return;
    }
    push([this] { drawFrame(); });
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