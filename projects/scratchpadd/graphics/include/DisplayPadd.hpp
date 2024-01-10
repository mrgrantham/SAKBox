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

public:
  void prepare() override {
    spdlog::info("Preparing: {}", name());
    // We dont want the work loop to sleep
    // TODO make this sleep/wake from semaphore
    work_thread_sleep_interval_ = 0ms;
    graphics_ = GraphicsBuilder();
    performanceTimer_.setTimerName(name());
    performanceTimer_.start();
  }
  DisplayPadd(ScratchPadd::System *system) : Base(system) {}

  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return 15666666ns;
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
                    .setFragmentShaderPath(getShaderPath("fragment"))
                    .setVertexShaderPath(getShaderPath("vertex"))
                    .build();
    graphics_->addView(std::move(view));
  }
  void finishing() override {
    spdlog::info("[{}] Avg Repeating Interval: {}", name(),
                 performanceTimer_.getAverageIntervalString());
    performanceTimer_.markTimeAndPrint();
    graphics_->tearDown();
  }

  virtual ~DisplayPadd() { spdlog::info("Destroying: {}", name()); }

  void repeat() override {
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
    }
  }

  std::string name() override { return __CLASS_NAME__; }

  bool shouldUseMainThread() override { return true; }

  void setupControlView(ScratchPadd::MessageType::ControlSnapshot &control) {
    auto controlView = ControlViewBuilder();
    controlView->setControls(control);
    std::cout << "controlView is: " << typeid(controlView).name() << '\n';
    // std::unique_ptr<Graphics::View> view(std::move(controlView));
    // graphics_->addView(std::move(controlView));
    graphics_->addView(
        dynamic_unique_cast<Graphics::View>(std::move(controlView)));
    // std::unique_ptr<Graphics::View> view =
    // dynamic_unique_cast<Graphics::View>(std::move(controlView));
  }

  void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::Triangle &message) {
                     std::cout << name() << "Triangle: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Point &message) {
                     std::cout << name() << "Point: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Text &message) {
                     std::cout << name() << "Text: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::ControlSnapshot &message) {
                     std::cout << name()
                               << "Control sourcename:" << message.paddName
                               << "\n";
                     setupControlView(message);
                   },
                   [&](auto &message) {
                     std::cout << name() << "Random Type: " << TypeName(message)
                               << "\n";
                   },
               },
               messageVariant);
  }
};