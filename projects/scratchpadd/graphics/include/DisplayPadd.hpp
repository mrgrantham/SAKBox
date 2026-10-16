#pragma once
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Graphics/Graphics.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/Timer.hpp>
using namespace std::chrono_literals;

class DisplayPadd : public ScratchPadd::Base {
private:
  ScratchPadd::Timer performanceTimer_;
  std::unique_ptr<Graphics> graphics_;
  // One control panel per padd with controls, by padd name. The views are
  // owned by graphics_; this map finds them to update in place.
  std::unordered_map<std::string, Graphics::ControlView *> controlViews_;

public:
  void prepare() override {
    spdlog::info("Preparing: {}", name());
    graphics_ = GraphicsBuilder();
    performanceTimer_.setTimerName(name());
    performanceTimer_.start();
  }
  DisplayPadd(ScratchPadd::System *system) : Base(system) {
    receive<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      std::cout << name() << " Text: " << text << "\n";
    });
    receive<ScratchPadd::MessageType::ControlSnapshot>(
        [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot) {
          showControls(snapshot);
        });
  }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {};
  }

  void starting() override {
    spdlog::info("Starting Window Setup for DisplayPadd");
    graphics_->setupWindow(1366, 768);
    PADDLOG_INFO("Window setup is complete");
    // auto view = std::make_unique<GL_View>();
    // GL_ViewBuilder viewBuilder;
    auto view = GetGL_ViewBuilder()
                    .setName("GLL Example")
                    .setFrameBuffer(800, 600)
                    .setSize(300, 300)
                    .setPosition(20, 40)
                    .setFragmentShaderPath(GetFragmentShaderPath("example"))
                    .setVertexShaderPath(GetVertexShaderPath("example"))
                    .setGeometry(SampleItems::vertices, SampleItems::indices)
                    .build();
    graphics_->addView(std::move(view));

    graphics_->addView(std::make_unique<Graphics::ImguiView>());

    // Ask every padd with controls for them; each reply becomes a panel.
    send(ScratchPadd::MessageType::ControlRequest{});

    push([this] { drawFrame(); }); // start drawing frames
  }
  void finishing() override {
    spdlog::info("[{}] Avg Repeating Interval: {}", name(),
                 performanceTimer_.getAverageIntervalString());
    performanceTimer_.markTimeAndPrint();
    graphics_->tearDown();
  }

  virtual ~DisplayPadd() { spdlog::info("Destroying: {}", name()); }

  // Draws one frame, then queues the next. The buffer swap waits for the
  // display's refresh (vsync), so this runs at the display's rate without a
  // timer and without spinning.
  void drawFrame() {
    performanceTimer_.markInterval();
    PADDLOG_INTERVAL(spdlog::level::info, 1000ms,
                     "DisplayPadd drawing at {:.3f}s interval",
                     performanceTimer_.getAverageIntervalInSeconds(5));
    PADDLOG_INTERVAL(spdlog::level::info, 1000ms,
                     "DisplayPadd drawing at {:.3f} fps",
                     1.0 / performanceTimer_.getAverageIntervalInSeconds(5));

    if (!graphics_->draw()) {
      spdlog::info("Stop drawing");
      system_->stop();
      return;
    }
    push([this] { drawFrame(); });
  }

  std::string name() override { return __CLASS_NAME__; }

  bool shouldUseMainThread() override { return true; }

  // Shows `snapshot` in that padd's control panel, creating the panel the
  // first time. Edits in the panel are sent back to the padd as
  // ControlChanges; its reply arrives here as the next snapshot.
  void showControls(const ScratchPadd::MessageType::ControlSnapshot &snapshot) {
    auto existing = controlViews_.find(snapshot.paddName);
    if (existing != controlViews_.end()) {
      existing->second->setControls(snapshot);
      return;
    }
    auto view = ControlViewBuilder();
    view->setControls(snapshot);
    view->setOnChange(
        [this](const ScratchPadd::MessageType::ControlChange &change) { send(change); });
    controlViews_[snapshot.paddName] = view.get();
    graphics_->addView(std::move(view));
  }
};