#pragma once

#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/MidiInput/MidiInput.hpp>

#include <rtmidi/RtMidi.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// MIDI from keyboards and controllers (desktops, with RtMidi: Core MIDI on
// macOS, ALSA on Linux, Windows MIDI). Listens on every MIDI input port,
// and opens new ones within a second of their being plugged in. Each event
// is broadcast as a MidiInput message and, with the playThrough control on
// (the default), played on AudioPadd's instrument.
//
// RtMidi calls back on its own thread; the callback only queues the event
// for this padd's thread.
class MidiInputPadd : public ScratchPadd::Base {
public:
  // Which MIDI input ports to listen on, by name: every one, unless set
  // otherwise before the system starts. (MIDI ports are shared by every
  // program on the machine; tests listen only to their own.)
  using PortFilter = std::function<bool(const std::string &name)>;
  static PortFilter &portFilter() {
    static PortFilter filter = [](const std::string &) { return true; };
    return filter;
  }

  MidiInputPadd(ScratchPadd::System *system) : Base(system) {}

  std::string name() override { return __CLASS_NAME__; }

  ScratchPadd::ControlValueMap generateControls() override {
    return {{"playThrough", ScratchPadd::ControlType::Boolean(true)},
            {"ports", ScratchPadd::ControlType::String("none")}};
  }

  void prepare() override { audio_ = addressOf("AudioPadd"); }

  // Looks for ports plugged in or out
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::seconds(1);
  }
  void starting() override { rescan(); }
  void repeat() override { rescan(); }
  void finishing() override { ports_.clear(); } // stops their callbacks

private:
  struct Port {
    MidiInputPadd *padd;
    std::string name;
    std::unique_ptr<RtMidiIn> in;
  };

  // Opens ports that have appeared and closes ones that have gone
  void rescan() {
    std::vector<std::string> names;
    try {
      if (!lister_) {
        lister_ = std::make_unique<RtMidiIn>();
      }
      for (unsigned int i = 0; i < lister_->getPortCount(); ++i) {
        std::string name = lister_->getPortName(i);
        if (portFilter()(name)) {
          names.push_back(std::move(name));
        }
      }
    } catch (const RtMidiError &error) {
      if (!reportedNoMidi_) {
        logger().warn("{}: no MIDI input on this machine: {}", paddName(), error.getMessage());
        reportedNoMidi_ = true;
      }
      return;
    }
    const bool firstScan = !scanned_;
    scanned_ = true;
    const auto closed = std::erase_if(ports_, [&](const auto &entry) {
      return std::find(names.begin(), names.end(), entry.first) == names.end();
    });
    bool opened = false;
    for (const std::string &name : names) {
      // (A port that has no name yet is still being set up: next time)
      if (!name.empty() && !ports_.contains(name)) {
        opened = open(name) || opened;
      }
    }
    if (firstScan || closed > 0 || opened) {
      std::string list;
      for (const auto &[name, port] : ports_) {
        list += (list.empty() ? "" : ", ") + name;
      }
      logger().info("{}: listening on {}", paddName(), list.empty() ? "no ports" : list);
      setControl("ports", ScratchPadd::ControlType::String(list.empty() ? "none" : list));
    }
  }

  // Opens port `name`, looked up again by the RtMidiIn that opens it (ports
  // can come and go since the list was made, moving the others' numbers)
  bool open(const std::string &name) {
    try {
      auto port = std::make_unique<Port>(Port{.padd = this, .name = name, .in = nullptr});
      port->in = std::make_unique<RtMidiIn>();
      for (unsigned int i = 0; i < port->in->getPortCount(); ++i) {
        if (port->in->getPortName(i) == name) {
          port->in->setCallback(&MidiInputPadd::received, port.get());
          port->in->ignoreTypes(/*sysex=*/true, /*time=*/true, /*sense=*/true);
          port->in->openPort(i, "ScratchPadd");
          ports_.emplace(name, std::move(port));
          return true;
        }
      }
      return false; // gone already
    } catch (const RtMidiError &error) {
      logger().warn("{}: can't open {}: {}", paddName(), name, error.getMessage());
      return false;
    }
  }

  // RtMidi's thread: queue the event for the padd's
  static void received(double, std::vector<unsigned char> *bytes, void *data) {
    auto *port = static_cast<Port *>(data);
    if (const auto event = ScratchPadd::Audio::MidiEvent::fromBytes(bytes->data(), bytes->size())) {
      port->padd->push(
          [padd = port->padd, event = *event, name = port->name] { padd->forward(event, name); });
    }
  }

  void forward(const ScratchPadd::Audio::MidiEvent &event, const std::string &port) {
    send(ScratchPadd::MidiInput::MidiInput{.event = event, .port = port});
    if (audio_ && control<ScratchPadd::ControlType::Boolean>("playThrough").value) {
      sendTo(audio_, ScratchPadd::Audio::PlayMidiEvent{.event = event});
    }
  }

  ScratchPadd::PaddAddress audio_;
  std::unique_ptr<RtMidiIn> lister_; // lists the ports
  std::map<std::string, std::unique_ptr<Port>> ports_;
  bool reportedNoMidi_ = false;
  bool scanned_ = false;
};
