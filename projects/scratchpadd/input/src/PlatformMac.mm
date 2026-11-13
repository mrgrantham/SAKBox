// The Mac's pads, through Apple's GameController. Pads are found only while
// the main thread's run loop runs (measured: a command-line process sees
// none without it), so a read on the main thread runs it for no time,
// handling what's pending; once found, a pad's values update by themselves.
#include <ScratchPadd/Input/Devices.hpp>

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

namespace ScratchPadd::Input {

Gamepad readPlatformPad() {
  @autoreleasepool {
    static const bool kStarted = [] {
      // (the window, or the terminal, may be the app in front)
      GCController.shouldMonitorBackgroundEvents = YES;
      return true;
    }();
    (void)kStarted;
    if ([NSThread isMainThread])
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);

    // The one used most recently, of several
    GCController *controller = GCController.current;
    if (!controller)
      controller = GCController.controllers.firstObject;
    GCExtendedGamepad *g = controller.extendedGamepad;
    if (!g)
      return {};
    NSString *product = controller.productCategory;
    return fromGameController(GameControllerState{
        .productCategory = product ? std::string_view(product.UTF8String) : std::string_view(),
        .bottom = g.buttonA.pressed,
        .right = g.buttonB.pressed,
        .left = g.buttonX.pressed,
        .top = g.buttonY.pressed,
        .leftShoulder = g.leftShoulder.pressed,
        .rightShoulder = g.rightShoulder.pressed,
        .menu = g.buttonMenu.pressed,
        .options = g.buttonOptions.pressed,
        .leftThumb = g.leftThumbstickButton.pressed,
        .rightThumb = g.rightThumbstickButton.pressed,
        .up = g.dpad.up.pressed,
        .down = g.dpad.down.pressed,
        .leftDpad = g.dpad.left.pressed,
        .rightDpad = g.dpad.right.pressed,
        .leftX = g.leftThumbstick.xAxis.value,
        .leftY = g.leftThumbstick.yAxis.value,
        .rightX = g.rightThumbstick.xAxis.value,
        .rightY = g.rightThumbstick.yAxis.value,
        .leftTrigger = g.leftTrigger.value,
        .rightTrigger = g.rightTrigger.value,
        .hasThumbButtons = g.leftThumbstickButton != nil,
        .hasOptions = g.buttonOptions != nil,
    });
  }
}

} // namespace ScratchPadd::Input
