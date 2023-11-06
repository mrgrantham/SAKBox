#pragma once
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Graphics/Graphics.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/Timer.hpp>
class DisplayPadd : public ScratchPadd::Base {
private:
  ScratchPadd::Timer performanceTimer_;
  std::unique_ptr<Graphics> graphics_;

public:
  virtual void prepare() override {
    spdlog::info("Preparing: {}", __CLASS_NAME__);
    setRepeatInterval(16); // 16 = ~60hz    32 = ~ 30hz
    performanceTimer_.setTimerName(paddName_);
    performanceTimer_.start();
  }
  DisplayPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", __CLASS_NAME__);
    paddName_ = __CLASS_NAME__;
    graphics_ = GraphicsBuilder();
    // We dont want the work loop to sleep
    // TODO make this sleep/wake from semaphore
    work_thread_sleep_interval_ = 0;
  }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant> generateControls() override {return {};}

  virtual void starting() override {
    spdlog::info("Starting Window Setup");
    graphics_->setupWindow();
  }
  virtual void finishing() override {
    spdlog::info("[{}] Avg Repeating Interval: {}", paddName_,
                 performanceTimer_.getAverageIntervalString());
    performanceTimer_.markTimeAndPrint();
    graphics_->tearDown();
  }

  virtual ~DisplayPadd() { spdlog::info("Destroying: {}", __CLASS_NAME__); }

  virtual void repeat() override {
    performanceTimer_.markInterval();
    static int count = 0;
    if (count % 30 == 0) {
      spdlog::info("DisplayPadd drawing at {:.3f}s interval",
                   performanceTimer_.getAverageIntervalInSeconds(5));
      spdlog::info("DisplayPadd drawing at {:.3f} fps",
                   1.0 / performanceTimer_.getAverageIntervalInSeconds(5));
    }
    count++;

    if (!graphics_->draw()) {
      spdlog::info("Stop drawing");
      system_->stop();
    }
  }

  virtual bool runOnMainThread() override { return true; }

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

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::Triangle &message) {
                     std::cout << paddName_ << "Triangle: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Point &message) {
                     std::cout << paddName_ << "Point: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Text &message) {
                     std::cout << paddName_ << "Text: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::ControlSnapshot &message) {
                     std::cout << paddName_
                               << "Control sourcename:" << message.paddName
                               << "\n";
                     setupControlView(message);
                   },
                  [&](auto &message) {
                     std::cout << paddName_ << "Random Type: " << TypeName(message) << "\n";
                   },},
               messageVariant);
  }
};