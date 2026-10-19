#pragma once
// Logs the demo's Triangle messages: an app-defined message type received
// with receive<T>(), without any change to the ScratchPadd library.
#include <DemoMessages.hpp>
#include <ScratchPadd/Base.hpp>
#include <iostream>

class ShapeLoggerPadd : public ScratchPadd::Base {
public:
  ShapeLoggerPadd(ScratchPadd::System *system) : Base(system) {
    receive<Demo::Triangle>([this](const Demo::Triangle &triangle) {
      std::cout << paddName() << " Triangle: " << triangle << "\n";
    });
  }
  std::string name() override { return __CLASS_NAME__; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};
