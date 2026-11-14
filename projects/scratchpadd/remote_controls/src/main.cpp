// remote_controls: a console's controls on the Mac. It connects to the
// RemoteControlsPadd of an app on a 3DS (or anything else running one) and
// shows every padd's controls there in the Controls window, as a desktop
// app's own; a change made here goes to the padd on the console, and the
// console's answer comes back.
//
//   bazel run //projects/scratchpadd/remote_controls -- 192.168.4.25   # a 3DS
//   bazel run //projects/scratchpadd/remote_controls                   # Azahar, on this Mac
//
// Until it's connected (the app not started yet, or gone) it tries again
// every 2 s, and says so.
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/DebugUi/ControlPanel.hpp>
#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Messages/ControlsText.hpp>
#include <ScratchPadd/Network/LineService.hpp>
#include <ScratchPadd/Network/NetworkPadd.hpp>
#include <ScratchPadd/Network/RemoteControlsPadd.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

namespace {

// Where to connect (main sets it from the command line)
std::string gHost = "127.0.0.1";
std::uint16_t gPort = ScratchPadd::Network::RemoteControlsPadd::kPort;

namespace ControlsText = ScratchPadd::ControlsText;

// The window: the console's controls, and whether it's connected
class RemoteWindowPadd : public ScratchPadd::Base {
public:
  explicit RemoteWindowPadd(ScratchPadd::System *system)
      : Base(system), link_("console", {.host = gHost, .port = gPort},
                            // (on the network loop's thread: handled on this padd's, the main one)
                            {.onLine =
                                 [this](std::uint32_t, std::string line) {
                                   push([this, line = std::move(line)] { handle(line); });
                                 },
                             .onConnected =
                                 [this](std::uint32_t) {
                                   link_.send(ControlsText::request());
                                   push([this] { connected_ = true; });
                                 },
                             .onDisconnected =
                                 [this](std::uint32_t) {
                                   push([this] {
                                     connected_ = false;
                                     panels_.clear();
                                   });
                                 }}),
        // A change made here: to the padd on the console, by its name
        panels_([this](ScratchPadd::PaddAddress, const std::string &padd,
                       const ScratchPadd::MessageType::ControlChange &change) {
          link_.send(ControlsText::change(padd, change));
        }) {}

  std::string name() override { return "RemoteWindow"; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    namespace Scene = ScratchPadd::Scene;
    display_ = std::make_unique<Scene::Display>(900, 700, "Remote controls: " + where());
    if (!display_->isOpen()) {
      logger().error("No window: {}", display_->error());
      system_->stop();
      return;
    }
    renderer_ = std::make_unique<Scene::Renderer>(*display_);
    ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_);
    ScratchPadd::Network::services().add(&link_); // (NetworkPadd connects it)
    push([this] { drawFrame(); });
  }
  void finishing() override {
    ScratchPadd::Network::services().remove(&link_);
    panels_.clear();
    ui_.reset();
    renderer_.reset();
    display_.reset();
  }

private:
  std::string where() const { return gHost + ":" + std::to_string(gPort); }

  void handle(const std::string &line) {
    const auto message = ControlsText::decode(line);
    if (message && message->kind == ControlsText::Decoded::Kind::Snapshot)
      panels_.show(message->snapshot, ScratchPadd::PaddAddress{}); // (a padd on the console)
  }

  void drawFrame() {
    namespace Scene = ScratchPadd::Scene;
    if (!display_->isOpen()) {
      system_->stop();
      return;
    }
    display_->pollEvents();
    ui_->newFrame();
    Scene::Environment environment;
    environment.background = {40, 32, 28};
    renderer_->beginFrame(Scene::Camera{}, environment);
    const std::string status =
        connected_ ? "Connected to " + where() : "Waiting for " + where() + " (tried every 2 s)";
    renderer_->overlay().text(12, 12, status.c_str(), {240, 230, 210}, 1.5f);
    panels_.draw();
    renderer_->endFrame();
    push([this] { drawFrame(); });
  }

  ScratchPadd::Network::LineService link_;
  ScratchPadd::DebugUi::ControlPanels panels_;
  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
  bool connected_ = false;
};

} // namespace

int main(int argc, char **argv) {
  if (argc > 1)
    gHost = argv[1];
  if (argc > 2)
    gPort = static_cast<std::uint16_t>(std::strtoul(argv[2], nullptr, 10));
  ScratchPadd::DebugLog::Session log; // (the terminal and a file)
  ScratchPadd::SystemImplementation<RemoteWindowPadd, ScratchPadd::Network::NetworkPadd> system;
  system.instantiate();
  system.start(); // returns when the window closes
  return EXIT_SUCCESS;
}
