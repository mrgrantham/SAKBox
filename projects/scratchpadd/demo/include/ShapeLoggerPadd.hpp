#pragma once
// Logs the demo's Triangle messages: an app-defined message type received
// with on<T>(), without any change to the ScratchPadd library.
#include <DemoMessages.hpp>
#include <ScratchPadd/Base.hpp>
#include <iostream>

class ShapeLoggerPadd : public ScratchPadd::Base {
public:
  ShapeLoggerPadd(ScratchPadd::System *system) : Base(system) {
    on<Demo::Triangle>([this](const Demo::Triangle &triangle) {
      std::cout << name() << " Triangle: " << triangle << "\n";
    });
  }
  std::string name() override { return __CLASS_NAME__; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};
