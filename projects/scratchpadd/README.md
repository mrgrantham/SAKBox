# ScratchPadd

ScratchPadd organizes a program as a set of independent subsystems, called **padds**, that talk to each other only by sending messages. Each padd has its own work queue and, by default, its own thread, so a padd's state is only ever touched by one thread and needs no locks.

It suits programs made of loosely coupled parts that run at different rates: for example a display that draws at 60 fps, a simulation that steps on a timer, and a control panel that tweaks both.

```
            ┌──────────────── System ────────────────┐
  send() ──►│ broadcast to every other padd's queue  │
            └──────┬──────────────┬──────────────┬───┘
                   ▼              ▼              ▼
              DisplayPadd     StoryPadd      AudioPadd
              (main thread)   (own thread)   (own thread)
```

## Quick Start

A padd subclasses `ScratchPadd::Base`. This one sends a message every second:

```cpp
#include <ScratchPadd/ScratchPadd.hpp>
using namespace std::chrono_literals;

class Ticker : public ScratchPadd::Base {
public:
  Ticker(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Ticker"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void receive(ScratchPadd::Message message) override {}

  // Call repeat() every second, on this padd's thread.
  std::optional<std::chrono::duration<double>> repeatInterval() override { return 1s; }
  void repeat() override {
    send(ScratchPadd::MessageType::Text{"tick " + std::to_string(++ticks_)});
  }

private:
  int ticks_ = 0;
};
```

And this one prints the messages it receives:

```cpp
class Printer : public ScratchPadd::Base {
public:
  Printer(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Printer"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void receive(ScratchPadd::Message message) override {
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::Text &text) {
                     logger().info("received {}", text.contents);
                   },
                   [](auto &) {}, // ignore every other message type
               },
               *message);
  }
};
```

A system is built from a list of padd types and runs until something calls `stop()`:

```cpp
int main() {
  ScratchPadd::System *system = ScratchPadd::SystemBuilder<Ticker, Printer>();
  system->instantiate(); // constructs every padd
  system->start();       // runs until system->stop() is called
}
```

Every padd must implement `name()` (unique within the system), `generateControls()` and `receive(Message)`, and have a constructor that takes a `ScratchPadd::System *`. These examples are compiled and run by `basis/tests/Test_Examples.cpp`.

To see a bigger system, run the demo: a window with a shader view and the Dear ImGui demo, plus a padd that logs on a timer. Close the window to quit.

```bash
bazel run //projects/scratchpadd/demo
```

[`shader_playground`](../examples/shader_playground) is another complete ScratchPadd app.

## Concepts

### Lifecycle

`instantiate()` constructs the padds and then reads their configuration once: `name()`, `repeatInterval()`, `shouldUseMainThread()` and `generateControls()`. Changing what those return later has no effect.

After that, each padd goes through these hooks. All except the constructor are optional overrides:

| Hook | When | Thread |
| --- | --- | --- |
| constructor | `instantiate()` | the thread calling `instantiate()` |
| `prepare()` | start of `start()`, before any padd thread exists | the thread calling `start()` |
| `starting()` | when the padd's work loop starts | the padd's thread |
| `repeat()` | every `repeatInterval()`, if set | the padd's thread |
| `receive()` | for each message | the padd's thread |
| `finishing()` | when the padd's work loop ends | the padd's thread |
| `cleanup()` | at shutdown, after every padd's `finishing()` | the thread calling `start()` |

Use `prepare()`/`cleanup()` for setup that must happen in order across padds, and `starting()`/`finishing()` for setup that belongs on the padd's own thread (an OpenGL context, for example).

### Threads

By default each padd gets its own thread, which processes its work queue: incoming messages and `repeat()` calls, one at a time, in order.

A padd whose `shouldUseMainThread()` returns `true` instead runs on the thread that called `start()`. Use this for anything that must run on the main thread, like windowing on macOS. All main-thread padds share one loop: each pass runs every one's queued work, then sleeps once, for the shortest of their sleep intervals.

`start()` blocks until `stop()` is called. `stop()` only requests shutdown and returns immediately, so it is safe from any thread, including from inside a padd: the demo's `DisplayPadd` calls `system_->stop()` when its window closes. `start()` then shuts down on its own thread, in order: every padd stops (background padds run `finishing()` on their own threads), main-thread padds run `finishing()`, and every padd runs `cleanup()`. `start()` returns once that is done. Tests start the system on a separate thread and call `waitForStart()` to wait until every padd is running.

When its queue is empty, a padd's thread sleeps for `work_thread_sleep_interval_` (500 ms by default) before checking again, so a message can take up to that long to be picked up. Padds that need lower latency can shorten it in their constructor or `prepare()`; `DisplayPadd` sets it to 0.

### Messages

A `ScratchPadd::Message` is a `std::shared_ptr` to a `std::variant` of every message type, so a broadcast shares one copy between all receivers instead of copying it. Treat a received message as read-only.

- `send(contents)` delivers to every padd except the sender.
- `sendIncludeSender(message)` delivers to every padd, including the sender.

Receive messages by overriding `receive(Message)` and using `std::visit`. `VariantHandler` combines lambdas into one visitor; end it with an `auto &` lambda to ignore the types you don't handle, as `Printer` does above.

The message types are listed in [`messages/include/ScratchPadd/Messages/Message.hpp`](messages/include/ScratchPadd/Messages/Message.hpp):

| Type | Contents |
| --- | --- |
| `Text` | a string |
| `Point`, `Triangle` | 2D geometry (`Geometry.hpp`) |
| `ControlRequest`, `ControlSnapshot`, `ControlChange` | the control system (below) |

**To add a message type**, define the struct in the `ScratchPadd::MessageType` namespace and add it to the `MessageTypes` list in `Message.hpp`. Every padd's `std::visit` must then either handle it or have a catch-all lambda.

### Controls

A padd can describe values it wants to be adjustable at runtime by returning them from `generateControls()`:

```cpp
ScratchPadd::ControlValueMap generateControls() override {
  return {
      {"speed", ScratchPadd::ControlType::Double(1.0, std::make_pair(0.0, 10.0))},
      {"enabled", ScratchPadd::ControlType::Boolean(true)},
  };
}
```

The control types are `Double` and `Integer` (each with an optional range), `Boolean`, and `String` (with an optional list of allowed values).

Controls are designed to be shown and edited by another padd, such as a UI panel, through three messages that `Base` handles automatically:

1. The panel sends a `ControlRequest`.
2. Every padd replies with a `ControlSnapshot` of its controls. The panel receives it by overriding `handleControlSnapshot()`.
3. To change a value, the panel sends a `ControlChange` naming the padd and control. That padd updates its value if the type matches, and the padds send fresh snapshots so the panel stays in sync.

A padd reads its current values from `generateControlsSnapshot().controlMap`. `messages/tests/Test_Controls.cpp` shows the whole round trip.

### Logging and Timing

Every padd has a `logger()` (spdlog) named after the padd. The `PADDLOG_INFO`, `PADDLOG_ERROR`, ... macros log through the current thread's logger, and `PADDLOG_INTERVAL` rate-limits a log line. `ScratchPadd::Timer` measures durations and averages intervals, and `SCOPED_METHOD_TIMER()` logs how long the enclosing function took.

## Graphics

The `graphics` package provides a window with Dear ImGui and OpenGL views. Create it in a main-thread padd:

```cpp
void prepare() override { graphics_ = GraphicsBuilder(); }

void starting() override {
  graphics_->setupWindow(1366, 768);
  graphics_->addView(GetGL_ViewBuilder()
                         .setName("Example")
                         .setFrameBuffer(800, 600)
                         .setSize(300, 300)
                         .setPosition(20, 40)
                         .setFragmentShaderPath(GetFragmentShaderPath("example"))
                         .setVertexShaderPath(GetVertexShaderPath("example"))
                         .setGeometry(SampleItems::vertices, SampleItems::indices)
                         .build());
}

void repeat() override { // with repeatInterval() set to about 16 ms
  if (!graphics_->draw()) // false once the window is closed
    system_->stop();
}

void finishing() override { graphics_->tearDown(); }
```

- Each `GL_View` renders a shader into a framebuffer shown in its own ImGui window. Shaders get Shadertoy-style uniforms: `iTime`, `iResolution`, `iMouse`, plus `uniform_color`.
- Shader files are Bazel data dependencies. Call `DataDepRetriever::ConfigureDependencies(argv[0])` at the start of `main()` so `GetFragmentShaderPath()` can find them, and pass a directory (relative to the repo root) for shaders outside this package, as `shader_playground` does.
- `Graphics::ImguiView` shows the Dear ImGui demo window.
- The backend is chosen at compile time. Only OpenGL (`SCRATCHPADD_GL`, set by the `graphics` target) exists so far.

## Packages

| Bazel target | Contents |
| --- | --- |
| `//projects/scratchpadd/basis` | The core: `Base`, `System`, `SystemBuilder`. Depend on this; it includes `messages` and `utilities` |
| `//projects/scratchpadd/messages` | Message and control types |
| `//projects/scratchpadd/utilities` | Logging, `Timer`, `EventTimer` and helpers like `VariantHandler` |
| `//projects/scratchpadd/graphics` | GLFW/ImGui/OpenGL window and views |
| `//projects/scratchpadd/audio` | Placeholder; `AudioPadd` only logs the messages it receives |
| `//projects/scratchpadd/demo` | The demo app |

## Building and Testing

```bash
bazel build //projects/scratchpadd/...
bazel test //projects/scratchpadd/...
bazel run //projects/scratchpadd/basis:clang_format   # format the basis package
```

| Test | Covers |
| --- | --- |
| `basis:Test_Examples` | The examples in this README |
| `basis:Test_Base` | Padd names, and a system starting and stopping |
| `basis:Test_Threading` | Padds run on the right threads; shutdown order; stopping from any thread, including inside a padd |
| `messages:Test_Messages` | Delivery between padds: contents intact, not echoed to the sender, ignored types skipped, `sendIncludeSender()` |
| `messages:Test_Controls` | The control request, snapshot and change round trip |
| `utilities:Test_EventTimer` | Repeating and one-shot timers, stopping |

## Known Limitations

- **Polling.** An idle padd checks its queue every `work_thread_sleep_interval_` instead of waking when work arrives, which adds latency (see [Threads](#threads)). Setting the interval to 0 removes the latency but keeps that thread busy.
- **Every message goes to every padd.** There is no subscription or direct addressing, and `Base` logs a spurious "no inner receive for this message type" error for each non-control message.
- **The message list is closed.** Adding a type means editing `Message.hpp` and rebuilding every padd, and app-specific types (`Point`, `Triangle`) live in the library.
- **Messages are shared but mutable.** Receivers get non-const references to the same object, so a receiver that modifies a message races with the others.
- **Controls are not wired up in the demo.** Nothing sends a `ControlRequest`, `DisplayPadd` doesn't implement `handleControlSnapshot()`, the control panel's text input writes past its string buffer, and edits aren't sent back as `ControlChange`. Every padd also sends a snapshot for every `ControlChange`, not only the one that changed.
- `sendIncludeSender(contents)`, the convenience overload that wraps the contents, doesn't compile if used; build the message with `MakeMsg()` and pass that instead.
- Audio is a stub.

## Roadmap

1. Wake padds with a condition variable when work arrives, instead of polling.
2. Make received messages `const`.
3. Let applications supply their own message types, e.g. as a template parameter, instead of editing the library.
4. Wire up the control panels in the demo and fix the issues listed above.
5. Use the cached `name_` everywhere instead of calling `name()` repeatedly.
6. Implement audio.
