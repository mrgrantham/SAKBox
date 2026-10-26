# ScratchPadd

ScratchPadd organizes a program as a set of independent subsystems, called **padds**, that talk to each other only by sending messages. Each padd has its own work queue and, by default, its own thread, so a padd's state is only ever touched by one thread and needs no locks.

It suits programs made of loosely coupled parts that run at different rates: for example a display that draws at 60 fps, a simulation that steps on a timer, and a control panel that tweaks both.

```
            ┌───────────────── System ──────────────────┐
  send() ──►│ queue for each padd that handles the type │
            └──────┬──────────────┬──────────────┬──────┘
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

  // Call repeat() every second, on this padd's thread.
  std::optional<std::chrono::duration<double>> repeatInterval() override { return 1s; }
  void repeat() override {
    send(ScratchPadd::MessageType::Text{"tick " + std::to_string(++ticks_)});
  }

private:
  int ticks_ = 0;
};
```

And this one prints the `Text` messages it receives, by registering a handler for that type:

```cpp
class Printer : public ScratchPadd::Base {
public:
  Printer(ScratchPadd::System *system) : Base(system) {
    receive<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      logger().info("received {}", text.contents);
    });
  }
  std::string name() override { return "Printer"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
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

Every padd must implement `name()` (unique within the system) and `generateControls()`, and have a constructor that takes a `ScratchPadd::System *`. These examples are compiled and run by `basis/tests/Examples_test.cpp`.

To see a bigger system, run the demo: a window with a shader view, the Dear ImGui demo, and a control panel for `StoryPadd`'s controls (edits are logged; `chapter` counts up by itself, and ticking `showExtra` adds an `extra` control), plus a demo-defined `Triangle` message logged by another padd. Close the window to quit.

```bash
bazel run //projects/scratchpadd/demo
```

[`shader_playground`](../examples/shader_playground) is another complete ScratchPadd app, and [Cyclone in padds](../examples/cyclone_padds/README.md) runs a physics simulation on its own thread, sending snapshots to a window padd that draws them blended at the screen's rate.

## Concepts

### Lifecycle

`instantiate()` constructs the padds and then reads their configuration once: `name()`, `repeatInterval()`, `runOn()` and `generateControls()`. Changing what those return later has no effect. In particular `name()` is called once: after that, use `paddName()`, which returns the cached name without calling it again (a `name()` built on `__CLASS_NAME__` demangles the class name on each call). In a padd's constructor, which runs before `instantiate()` reads the name, `paddName()` is still `"Unnamed Padd"`, so call `name()` there. The system itself uses `paddName()`, and a message's type name (`typeName()`, logged by `send()`) is worked out once per type, so sending a message doesn't call either.

After that, each padd goes through these hooks. All except the constructor are optional overrides:

| Hook | When | Thread |
| --- | --- | --- |
| constructor | `instantiate()` | the thread calling `instantiate()` |
| `prepare()` | start of `start()`, before any padd thread exists | the thread calling `start()` |
| `starting()` | when the padd's work loop starts | the padd's thread |
| `repeat()` | every `repeatInterval()`, if set | the padd's thread |
| `receive<T>()` handlers | for each message of type T | the padd's thread |
| `finishing()` | when the padd's work loop ends | the padd's thread |
| `cleanup()` | at shutdown, after every padd's `finishing()` | the thread calling `start()` |

Use `prepare()`/`cleanup()` for setup that must happen in order across padds, and `starting()`/`finishing()` for setup that belongs on the padd's own thread (an OpenGL context, for example).

### Threads

A padd says which thread its work runs on by what its `runOn()` returns, a `ScratchPadd::RunOn` (in `ScratchPadd/Threads.hpp`):

| `runOn()` returns | The padd runs on |
| --- | --- |
| `RunOn::BackgroundThread` (the default, where there are threads) | a thread of its own, which processes its work queue: incoming messages and `repeat()` calls, one at a time, in order |
| `RunOn::MainThread` | the thread that called `start()`. Use this for anything that must run on the main thread, like windowing on macOS. All main-thread padds share one loop: each pass runs every one's queued work, in the order the padds are listed, then sleeps until work is pushed to any of them |

```cpp
class DisplayPadd : public ScratchPadd::Base {
public:
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; } // windowing
  ...
};
```

`runOn()` is read once, when the system is built. A padd that doesn't override it gets `kDefaultRunOn`, which suits the platform (below).

**Platforms without threads.** Some have none: devkitPro's libnds for the Nintendo DS has no `std::thread`. `Threads.hpp` works out at compile time whether the C++ library has threads, as `SCRATCHPADD_HAS_THREADS` (1 or 0; `ScratchPadd::kHasThreads` in code): 0 where libstdc++ lacks `_GLIBCXX_HAS_GTHREADS` or libc++ has threads turned off, on the DS (`__NDS__`), and wherever `SCRATCHPADD_NO_THREADS` is defined. Where it's 0:

- `RunOn::BackgroundThread` doesn't exist, so **a padd whose `runOn()` returns it doesn't compile** (the compiler says `no member named 'BackgroundThread' in 'ScratchPadd::RunOn'`). A padd can't ask for a thread the platform can't give it.
- `kDefaultRunOn` is `RunOn::MainThread`, so a padd that doesn't override `runOn()` builds and runs on either kind of platform.
- `RunOnUnthreaded_test` builds with `SCRATCHPADD_NO_THREADS` and checks both. That's the check for now: the rest of ScratchPadd (its locks, and the timer threads behind `repeat()`) still uses threads, so it doesn't build for the DS yet; that's the roadmap's next step for the DS.

`start()` blocks until `stop()` is called. `stop()` only requests shutdown and returns immediately, so it is safe from any thread, including from inside a padd: the demo's `DisplayPadd` calls `system_->stop()` when its window closes. `start()` then shuts down on its own thread, in order: every padd stops (background padds run `finishing()` on their own threads), main-thread padds run `finishing()`, and every padd runs `cleanup()`. `start()` returns once that is done. Tests start the system on a separate thread and call `waitForStart()` to wait until every padd is running.

**Padds don't poll.** A padd with nothing to do sleeps, using no CPU, and wakes the moment work is pushed to it: a message, a `repeat()` tick, or `stop()`. Work runs in the order it was pushed, and a job can push more work (to its own padd too). For periodic work use `repeatInterval()`; for work that should run as often as something else allows, such as drawing at the display's refresh rate, have each run `push()` the next one, as `DisplayPadd` does below.

### Messages

Any copyable struct can be a message. Define it in your own code, register a handler for it with `receive<T>()`, and send it with `send()`:

```cpp
struct GatePassed { int gate; };

// In a padd's constructor: runs on this padd's thread for every GatePassed
receive<GatePassed>([this](const GatePassed &event) { logger().info("gate {}", event.gate); });

// In any other padd
send(GatePassed{3});
```

- `send(contents)` delivers to every *other* padd that has a handler for the type; `sendIncludeSender(contents)` includes the sender. Padds without a handler never see the message, not even in their queue. A broadcast that no padd handles is logged as a warning the first time for its type (later ones of that type are dropped without one, since some broadcasts, such as a padd's control snapshots with no panel open, are routinely unheard); `SystemImplementation::unhandledMessageTypes()` lists those types.
- Register handlers in the constructor or `prepare()`, or change them while the padd runs (see below).
- A message is created once and shared by every receiver, so handlers get it as a `const` reference.
- To send one prepared message several times, wrap it once with `makeMessage(contents)` and pass the resulting `ScratchPadd::Message`. `messageAs<T>(message)` unwraps it, or returns null if it holds another type.

**Changing handlers while running.** A running padd can start handling a type with `receive<T>()` and stop with `stopReceiving<T>()` (which returns `false` if it had no handler for T). Call them on the padd's own thread, for example from a handler or `repeat()`; a handler may even replace or remove itself.

```cpp
// A padd that listens for one GatePassed, then ignores the rest
receive<GatePassed>([this](const GatePassed &event) {
  logger().info("first gate: {}", event.gate);
  stopReceiving<GatePassed>();
});
```

- Whether a padd gets a message is decided when the message is sent. Messages sent before `receive<T>()` never arrive; those already queued when `stopReceiving<T>()` runs are dropped when their turn comes, and the handler that was removed never runs again.
- Senders check a padd's handlers from their own threads, so the handlers are guarded by a read-write lock: sending takes a shared lock, changing them an exclusive one. Each handler is held by `shared_ptr`, so a handler runs after the lock is released and can't be destroyed while it runs.
- `Base` registers handlers for the control messages itself; stopping those would stop the padd answering control panels.

**Sending to one padd.** `sendTo` sends a message to one padd and no other. Name the padd by its class (each class appears once in a system), or get an address once and keep it:

```cpp
sendTo<DisplayPadd>(Text{"hello"});                 // by class
PaddAddress story = addressOf("StoryPadd");          // by name, looked up once (from prepare() on)
sendTo(story, GatePassed{3});                        // no lookup at all
```

- It returns `false`, sending nothing, if there's no such padd or the padd has no handler for the type; both are logged as errors. An empty `PaddAddress` (from a failed lookup) is `false`.
- A padd can send to itself (`sendTo(address(), ...)`); the message is queued like any other.
- Messages from one padd to another arrive in the order they were sent, whichever way they're sent.
- There's no "include the sender" version: a direct send names its one receiver.
- By class it's one hash lookup of the class's `type_index`; with a kept address, none. Names are only compared by `addressOf(name)`, which is why to keep the address rather than look it up per message.

**Who sent a message.** A handler can take the sender's address as a second argument, and reply to it:

```cpp
receive<Ping>([this](const Ping &ping, PaddAddress from) { sendTo(from, Pong{ping.id}); });
```

- Every send records its sender, broadcasts included. A message sent from outside any padd (`System::sendIncludeSender(nullptr, ...)`) has an empty `from`.
- One-argument handlers (`[](const Ping &ping) {...}`) are unchanged; `receive` works out which kind it was given when it compiles.
- The sender travels with each delivery, not inside the message, since one message (from `makeMessage`) can be sent more than once, by different padds.
- `reply(from, contents)` answers only if the sender handles the answer's type, and otherwise sends nothing, without an error. A padd that answers whoever asks uses it, so askers that don't care about the answer needn't handle it. (`sendTo` logs an error for a padd that doesn't handle the type, since a direct send is meant to arrive.) `AudioPadd` answers this way.

**Padds only talk through messages.** A padd's state belongs to its own thread, so other code may not call into a padd; it sends the padd a message. This is enforced, not just a convention:

- *At compile time.* What a padd does is protected in `Base`: `receive`, `send`, `sendTo`, `reply`, `push`, and its controls (`control`, `setControl`, `addControl`, `removeControl`, `updateControl`). C++ only lets a class use protected members of objects of its own class, so one padd can't call another's even through a pointer to it. Running a padd (`run`, `stop`, `deliver`, `initialize`...) is private to `Base` and the system. What stays public only reads a padd or waits for it: `paddName()`, `isRunning()`, `pendingWorkCount()`, `workLoopPasses()`, `waitForStart()`, `waitForStop()`, `waitForWorkCompletion()`.
- *Addresses can't be opened.* A `PaddAddress` can be copied, compared, sent to and named, but has no way to get at the padd behind it. The system hands out padds only as addresses.
- *At run time.* The control methods check they're on the padd's own thread (while it runs: from `starting()` to `finishing()`), and throw `std::logic_error` naming the padd and method if not. Call `assertOnPaddThread("method")` at the top of your own padd methods that touch its state.
- *What it can't stop.* C++ can't stop a deliberate workaround (a pointer kept in a global, a cast), and public methods you add to your own padd class are yours to guard: keep them private, or check the thread.
- `Messages_test` checks the compile-time rules when it compiles, so loosening one fails the build.

**Tests** reach inside padds through `ScratchPadd::PaddTestAccess` (`ScratchPadd/Testing.hpp`): sending as if from a padd, queueing work on it, and its controls. `onPaddThread(padd, work)` runs `work` on the padd's own thread and returns its result, for reading a running padd's controls. A test's own padd class can also make a protected method public with a `using` declaration.

The library defines only general-purpose messages in [`messages/include/ScratchPadd/Messages/`](messages/include/ScratchPadd/Messages/): `Text` (a string) and the three control messages below. The demo's `Triangle` (`demo/include/DemoMessages.hpp`) is an example of an app-defined message.

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

Read a control's current value on the padd's own thread with `control<C>(name)`, and override `controlChanged(name)` to react when one is edited:

```cpp
double speed = control<ScratchPadd::ControlType::Double>("speed").value;

void controlChanged(const std::string &name) override {
  if (name == "speed") { /* use the new speed */ }
}
```

`generateControls()` gives the controls a padd starts with. To change them while it runs, call these on the padd's own thread:

| Call | Does |
| --- | --- |
| `setControl(name, value)` | Sets a control's value, by the same rules as an edit from a panel (clamped to its range, an allowed string, the same type). Returns `false`, changing nothing, if the value isn't allowed or there's no such control. |
| `addControl(name, control)` | Adds a control, or replaces the one with that name, range and all. A padd that had none gets a panel. |
| `removeControl(name)` | Removes a control. Returns `false` if there was none. Removing the last one removes the padd's panel. |

```cpp
void repeat() override {
  setControl("chapter", ScratchPadd::ControlType::Integer(++chapter_)); // panels show it now
}
```

Each sends a fresh `ControlSnapshot`, so panels show the change straight away. None calls `controlChanged()`, which reports edits made from a panel; the padd already knows about its own changes. The demo's `StoryPadd` uses all three.

**The control panel.** `DisplayPadd` shows every padd's controls in its window, one ImGui panel per padd: sliders for ranged numbers, input boxes for unranged ones, checkboxes, and a combo box or text field for strings. A padd gets a panel just by returning controls from `generateControls()`.

Behind the panel are three messages; `Base` handles the owner's side automatically:

1. **Discover.** The panel broadcasts a `ControlRequest` (`DisplayPadd` does this once its window is up). Every padd with controls replies to the panel that asked, and only that panel, with a `ControlSnapshot`: its name and all its controls. To ask one padd, send the request to it with `sendTo`.
2. **Edit.** When the user edits a widget, the panel sends a `ControlChange` (the control's name and the new value) straight to the padd the snapshot came from: the snapshot handler takes the sender's address, and the panel keeps it.
3. **Apply and confirm.** The owner enforces its own rules: numbers are clamped to the control's range, strings must be one of the allowed values, and the type must match; only the value is taken, so a change can't alter a range. If the change is valid it's stored and `controlChanged()` is called. Either way the owner replies to every panel with a fresh snapshot, so they all show the value the owner actually holds (a clamped number snaps back, for example).

The panel only ever holds copies from snapshots, and each owner changes its own values on its own thread, so no locking is needed. A snapshot with no controls (sent when a padd removes its last one) removes that padd's panel; `DisplayPadd` takes it away with `Graphics::removeView`. `messages/tests/Controls_test.cpp` covers the round trip, range enforcement, rejection and who replies.

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
  push([this] { drawFrame(); }); // start drawing
}

// Queued once at the end of starting(); each frame queues the next. The
// buffer swap waits for the display's refresh, so frames run at the
// display's rate with no timer and no spinning.
void drawFrame() {
  if (!graphics_->draw()) { // false once the window is closed
    system_->stop();
    return;
  }
  push([this] { drawFrame(); });
}

void finishing() override { graphics_->tearDown(); }
```

- Each `GL_View` renders a shader into a framebuffer shown in its own ImGui window. Shaders get Shadertoy-style uniforms: `iTime`, `iResolution`, `iMouse`, plus `uniform_color`.
- Shader files are Bazel data dependencies. Call `DataDepRetriever::ConfigureDependencies(argv[0])` at the start of `main()` so `GetFragmentShaderPath()` can find them, and pass a directory (relative to the repo root) for shaders outside this package, as `shader_playground` does.
- `Graphics::ImguiView` shows the Dear ImGui demo window.
- `Graphics::addView` adds a view and `Graphics::removeView` destroys and removes one; call them on the thread that draws, between frames (as `DisplayPadd`'s message handlers are).
- The backend is chosen at compile time. Only OpenGL (`SCRATCHPADD_GL`, set by the `graphics` target) exists so far.
- `DisplayPadd` opens each padd's control panel at the next spot along the bottom of the window (left, middle, right, then top right), so they don't open on top of each other. ImGui saves where windows were to `imgui.ini` in the working directory and puts them back there next time, so to see the default places again, delete that file.

### The look

Every ScratchPadd window uses one look for Dear ImGui (`ScratchPadd/Graphics/Theme.hpp`), set once after the ImGui context is made:

```cpp
ImGui::CreateContext();
Theme::apply(Theme::Colors::Dark); // or Theme::Colors::Light
```

| | |
| --- | --- |
| Colors | A mid-century palette: walnut, charcoal and cream, with teal title bars, orange for sliders, check marks and pressed buttons, and mustard for headers and selections. `Dark` is walnut with cream text; `Light` is cream and sand with dark text. Each is one `Palette` of 13 colors in `Theme.cpp`, from which every one of ImGui's colors is set, so changing the look means changing those. Windows are opaque, so panels that overlap don't show through each other. |
| Spacing | Roomy: 12 by 10 px around a window's contents, 8 by 5 inside frames, 10 by 7 between items; corners rounded 4 px (windows 6) |
| Titles | **Futura** Medium, 17 px, where the system has it (macOS's own, `/System/Library/Fonts/Supplemental/Futura.ttc`, read when the program starts; Futura's license doesn't let it be shipped), and otherwise **Jost** Medium, a free font made in Futura's spirit |
| Text | **Questrial**, 15 px: a free geometric sans of the same family, chosen over Jost, Poppins and Didact Gothic because it stays legible at small sizes (its lowercase letters are taller) |

- **Titles in the title font:** ImGui draws a window's title with whatever font is current when the window begins, so `Theme::beginWindow(name, open, flags)` begins it with the title font and then goes back to the text font. Use it in place of `ImGui::Begin()` (and still call `ImGui::End()`). `Theme::heading(text)` writes a heading inside a window in the title font, and `Theme::titleFont()` gives the font itself. ScratchPadd's views and control panels, Precision Flight and Cyclone in padds use them; windows that call `ImGui::Begin()` directly (the ImGui demo's) get text-font titles.
- **Fonts are compiled in.** Jost and Questrial (both under the SIL Open Font License) are downloaded by Bazel, pinned by checksum in `MODULE.bazel`, and turned into C++ arrays by [`tools/embed_file`](../../tools/embed_file/README.md) when the graphics library is built, so a program never has to find a font file. Their licenses are downloaded with them and travel in the runfiles of every program that uses the graphics library.
- **Sizes stay sharp on high-resolution screens:** ImGui 1.92 and later draw fonts at the screen's own resolution, so nothing needs scaling by hand.

## Audio

`AudioPadd` plays sounds and music for the other padds, which ask it with messages. It runs on macOS, Linux and Windows through the system's sound device, and its engine (the audio core) also plays on the Nintendo 3DS, through its DSP.

```cpp
#include <ScratchPadd/Audio/Messages.hpp> // from //projects/scratchpadd/audio:audio_messages
using namespace ScratchPadd::Audio;

// In prepare(): find AudioPadd once (an empty address if the system has none)
audio_ = addressOf("AudioPadd");

// Load a file, or samples made in code, once
sendTo(audio_, LoadSound{.name = "door", .path = "sounds/door.wav"});
// Then play it, as often as wanted
sendTo(audio_, PlaySound{.name = "door", .settings = {.volume = 0.8f, .pan = -0.3f}});
sendTo(audio_, PlayMusic{.path = "music/theme.mp3", .settings = {.loop = true}});
```

| Message | Does | Answer |
| --- | --- | --- |
| `LoadSound{name, path}` | Loads a WAV, FLAC or MP3 file as a named sound, replacing any of that name | `SoundLoaded{name, ok, error}` |
| `LoadSoundSamples{name, samples, rate, channels}` | Loads samples made in code (a `shared_ptr` to interleaved floats, so sending copies nothing) | `SoundLoaded` |
| `UnloadSound{name}` | Unloads a sound; its voices stop at once | |
| `PlaySound{name, settings}` | Plays a loaded sound | `Playing{name, voice, ok, error}`, then `VoiceFinished{name, voice}` when it ends |
| `PlayMusic{path, settings}` | Streams a music file, decoded as it plays | `Playing`, then `VoiceFinished` |
| `StopVoice{voice}`, `SetVoice{voice, settings}` | Fades one voice out, or changes how it plays | |
| `StopMusic{}`, `StopAll{}` | Fades out all music (MIDI files too), or everything | |
| `PlayMidi{path, settings}` | Plays a MIDI file (`.mid`) on the instrument; one at a time | `Playing`, then `VoiceFinished` |
| `PlayMidiEvent{event}` | Plays a MIDI event on the instrument now, e.g. `MidiEvent::noteOn(0, 60, 100)` | |
| `LoadSoundFont{path}` | Plays MIDI with a SoundFont's instruments from now on | `SoundFontLoaded{path, ok, error}` |
| `UseBuiltInInstrument{}` | Goes back to the built-in instrument | |

- `settings` (`VoiceSettings`): `volume` (1 is as recorded), `pan` (a balance: -1 is left only, 0 both sides as recorded, 1 right only), `pitch` (playback speed: 2 is an octave up and half as long), and `loop`.
- Answers go back to the padd that asked, with `reply()`: handle `Playing`, `VoiceFinished` or `SoundLoaded` to hear them, or don't. They come in the order things were asked. The demo's `StoryPadd` plays a tune on a vibraphone, a note a chapter, and a chime made in code every fifth, without listening for answers.
- Controls: `master`, `sounds` and `music` volume (0 to 1), and `mute`. Sounds and music are mixed in two groups, each with its own volume under the master volume.
- Padds that only send these messages depend on `:audio_messages`, not the engine. AudioPadd itself is in `:audio`.
- Sound files are found from the working directory, like any other file. Loading decodes the whole file (and converts it to the output's rate) on AudioPadd's thread, so load sounds before they're needed. Music is decoded half a second ahead as it plays, so it can be long.

**MIDI.** MIDI files and events play on one instrument:

- **The built-in instrument** (`BuiltInSynth`) needs no files, so MIDI works everywhere from the start. Each of General MIDI's 16 instrument families (pianos, organs, guitars, strings, brass, synths, ...) has its own simple tone and envelope, and channel 10 plays synthesized drums. It follows volume, expression, pan, sustain, pitch bend (two semitones), all-notes-off and all-sound-off. It's for sketching and for the 3DS; it doesn't sound like real instruments.
- **A SoundFont** (`.sf2`, or the Ogg-compressed `.sf3` on desktops) sounds like one, played with [TinySoundFont](https://github.com/schellingb/TinySoundFont) (MIT). No SoundFont is in the repo. The demo uses [FluidR3Mono](https://github.com/musescore/MuseScore/tree/v3.6.2/share/sound) (MIT, General MIDI, 24 MB), which Bazel downloads only when the demo is built (`@fluidr3_mono_soundfont`, pinned by checksum; its license is downloaded beside it). Loading reads every sample into memory as floats: FluidR3Mono takes about 4 s and 250 MB, and AudioPadd's thread does nothing else meanwhile (sounds and MIDI already playing carry on; streamed music can run dry). On a 3DS, use the built-in instrument or a small `.sf2`.
- A MIDI file is read into a sequence when played and timed on the audio thread, in 32-frame steps, so each event lands within a millisecond of its time whatever AudioPadd is doing. Its `settings`: volume and pan as for any voice (applied to the instrument), `pitch` is speed, `loop` starts it over (letting hanging notes go first). Events and files play in the Music group.
- Levels: a chord at velocity 100 peaks around half of full scale on the built-in instrument. SoundFonts play 4 dB down, so FluidR3 averages about as loud (its instruments' own levels vary by over 15 dB; the limiter catches the loudest).
- To hear the instruments, `bazel run //projects/scratchpadd/audio:render_instrument_samples` writes WAV files to `.audio-samples/` in the repo (git-ignored): each of the 16 families playing an arpeggio and a held chord, and the drums, on the built-in instrument and on FluidR3.
- The instrument plays up to 48 notes at once (`EngineConfig::instrumentNotes`); more take over the oldest or quietest. Everything either instrument needs is allocated when it's made, so playing allocates nothing on the audio thread (TinySoundFont's voices and channels too; the tests check it).

**How it works.** The engine (`audio/core`) is two halves joined by two wait-free queues:

- The *mixer* runs on the sound device's thread and only mixes: it never locks, allocates, frees memory or waits, since a late buffer is heard as a click. It reads commands (play, stop, set, volumes) from one queue and reports voices that ended on the other.
- The *engine* runs on AudioPadd's thread and does the slow work: decoding and resampling files when they're loaded (with [miniaudio](https://miniaud.io)), decoding music ahead into a ring buffer, and freeing memory once the mixer says it's done with it. AudioPadd calls its `update()` every 20 ms.
- **When every voice is busy** (`EngineConfig::maxVoices`, 32 by default), a new sound takes over the quietest one playing (the oldest of the quietest), which fades out over one block and is reported finished; a new sound quieter than all of them is refused instead. Music and MIDI are never taken over, and take over a sound when they need a voice. So a burst of sounds keeps its loudest, newest ones, as game audio does. `EngineConfig::whenVoicesFull = WhenVoicesFull::Refuse` refuses every new sound instead. (Found with the Cyclone demos' knocks: refusing kept the burst's first, quiet hits and lost the loud ones after.)
- **Sounds made in code** (`ScratchPadd::Audio::MadeSounds`, the `:audio_made_sounds` library, which needs nothing else): a chime, a knock and a blast, each peaking at 0.9, to play without sound files. The demo's StoryPadd, the 3DS demo and the Cyclone demos use them.
- **A limiter on the whole mix** keeps it from clipping: where sounds add up past full scale (a loud SoundFont instrument, many sounds at once), it turns the mix down at once, then gives the gain back over about 0.1 s. Below full scale the mix passes through untouched. `MixerStats::limitedBlocks` counts the blocks it acted on.
- Sounds are resampled once, when loaded, so mixing is a copy, or a linear interpolation for a pitched voice. Volume and pan changes, and stops, ramp over one block (up to 1024 frames) rather than jump, so they don't click.
- `Output` is where the mix goes: `DeviceOutput` plays through miniaudio's device (Core Audio, PulseAudio/ALSA/JACK, WASAPI; with no sound device it falls back to miniaudio's silent null device, so the engine still runs), and `OfflineOutput` mixes only when asked, which the tests use to check exact samples. Tests swap AudioPadd's output with `AudioPadd::outputFactory()`.
- **On a 3DS**, `NdspOutput` ([`audio/3ds/output`](audio/3ds/output/include/ScratchPadd/Audio/NdspOutput.hpp)) plays through the DSP with libctru's ndsp, at the DSP's own rate (about 32,728 Hz, so nothing is resampled on the way out), in four 512-frame buffers (16 ms each, 64 ms in all). The DSP's callback only wakes a mixing thread, one priority step above the app's, which refills the buffers the DSP has finished, as SDL's 3DS audio does; it counts underruns and times each mix. The app's own loop (the owner) calls `Engine::update()` each frame. The 3DS has no padd system yet, so apps use the engine directly. ndsp needs the DSP's firmware at `sdmc:/3ds/dspfirm.cdc`, dumped from the console with DSP1; without it the engine reports so and stays silent.
- `MixingScope` (`RealTime.hpp`) marks the audio thread while it mixes, and allocations made there are counted: TinySoundFont's report themselves, and tests report every `new`. The tests check the count stays at zero.
- The core is plain C++ with no other ScratchPadd dependency, and doesn't throw or need RTTI, so it builds for the 3DS. On a 3DS, [`audio/3ds/selftest`](audio/3ds/selftest/BUILD) runs the core's checks on the console's own CPU and times a mix: 16 pitched voices take 94 ms per second of audio in the Azahar emulator, and 16 notes of the built-in instrument 113 ms. [`audio/3ds/demo`](audio/3ds/demo/BUILD) plays through the DSP by itself: a tune on the built-in instrument and a chime every two seconds, with the output's figures on the top screen (buffers queued and mixed, underruns, mix time, voices, notes) and in `scratchpadd-audio-3ds-demo.txt` on the SD card. In Azahar it plays with no underruns, mixing each 15.6 ms buffer in under 2 ms. `bazel run //projects/scratchpadd/audio/3ds/demo:scratchpadd-audio-3ds-demo` opens it. It shows the results on the top screen and writes them to `scratchpadd-audio-selftest.txt` on the SD card. `bazel run //projects/scratchpadd/audio/3ds/selftest:scratchpadd-audio-selftest` builds it (needs Docker; see [`tools/devkitpro`](../../tools/devkitpro/README.md)) and opens it in the Azahar emulator, which Bazel downloads; the results file is then in `.azahar/user/sdmc` (see [`tools/azahar`](../../tools/azahar/README.md)).

miniaudio (public domain or MIT-0), TinySoundFont (MIT) and stb_vorbis (public domain or MIT) are downloaded by Bazel; nothing needs installing. On Linux it finds PulseAudio or ALSA when the program runs, so no development packages are needed to build.

### MIDI input

`MidiInputPadd` (`//projects/scratchpadd/midi_input`) plays MIDI keyboards and controllers, on desktops, with [RtMidi](https://github.com/thestk/rtmidi) 6.0.0 (MIT-style, downloaded by Bazel): Core MIDI on macOS, ALSA on Linux, Windows MIDI on Windows.

- It listens on every MIDI input port, and opens ones plugged in later within a second (it looks again every second, by port name), letting go of ones unplugged. Its `ports` control lists them. To listen on only some, set `MidiInputPadd::portFilter()` (a test of each port's name) before the system starts. MIDI ports belong to the whole machine, not one program, so the tests use it to hear only their own virtual keyboard: Bazel runs tests side by side, and without it each would play the others' notes.
- Each note, controller or other channel message is broadcast as `MidiInput{event, port}` (`:midi_input_messages`), for any padd to handle, and, with the `playThrough` control on (the default), played on AudioPadd's instrument with `PlayMidiEvent`. The demo includes it, so a keyboard plugged in plays the demo's instrument (FluidR3Mono once loaded).
- System messages (clock, SysEx, active sensing) are left out.
- RtMidi calls back on its own thread; the callback only queues the event for the padd's thread.
- On a machine with no MIDI system (Linux without ALSA's sequencer, as in a container) it logs that once and carries on; its tests are skipped there.
- Building on Linux needs ALSA's development package, `libasound2-dev` (see the top-level README's Linux notes).

## Packages

| Bazel target | Contents |
| --- | --- |
| `//projects/scratchpadd/basis` | The core: `Base`, `System`, `SystemBuilder`. Depend on this; it includes `messages` and `utilities` |
| `//projects/scratchpadd/messages` | The message wrapper (`Message`, `makeMessage`), `Text`, and the control types |
| `//projects/scratchpadd/utilities` | Logging, `Timer`, `EventTimer` and helpers like `VariantHandler` |
| `//projects/scratchpadd/graphics` | GLFW/ImGui/OpenGL window and views, and the ImGui theme (`Theme.hpp`) |
| `//projects/scratchpadd/audio` | `:audio` (`AudioPadd`), `:audio_messages` (what padds send it), `:audio_core` (the engine and instruments, also built for the 3DS), `:audio_device` (playback through the sound device) |
| `//projects/scratchpadd/midi_input` | `MidiInputPadd` (MIDI keyboards and controllers, desktops) and `:midi_input_messages` |
| `//projects/scratchpadd/demo` | The demo app |

## Building and Testing

```bash
bazel build //projects/scratchpadd/...
bazel test //projects/scratchpadd/...
python3 tools/clang_format/format.py --all           # format the repo's C++ (see the top-level README)
```

Each test file is named after what it tests, as `<Source>_test.cpp` (see the top-level README's [Code conventions](../../README.md#code-conventions)).

| Test | Covers |
| --- | --- |
| `basis:Examples_test` | The examples in this README |
| `basis:Base_test` | Padd names, and a system starting and stopping |
| `basis:RunOn_test`, `basis:RunOnUnthreaded_test` | The same test built with threads and without (`SCRATCHPADD_NO_THREADS`): `RunOn::BackgroundThread` exists only with threads, and a padd that doesn't override `runOn()` gets a background thread with threads and runs on `start()`'s thread without |
| `basis:Threading_test` | Padds run on the right threads; shutdown order; stopping from any thread, including inside a padd; idle padds don't spin; work wakes a padd promptly; concurrent senders lose nothing |
| `messages:Messages_test` | App-defined messages: contents intact, not echoed to the sender, each type to its handler, padds without a handler never queued, `sendIncludeSender()`, one shared object per message, `name()` called once rather than per message, type names worked out once per type, handlers started and stopped while running (including a handler removing or replacing itself, queued messages of a removed type dropped, and senders racing handler changes); `sendTo` (only the named padd, by class and by name, to itself, in order, refused for a missing padd, an empty address or an unhandled type, and four padds sending to one at once from their own threads losing nothing, each in order and with the right sender); the sender (a reply to it, `reply()` reaching only senders that handle the answer, and no sender for a message from outside); broadcasts nobody handles noted once per type; and, when it compiles, that code outside a padd can't call into one |
| `messages:Controls_test` | The control round trip: requests answered to the panel that asked (all padds, or one sent the request), changes sent straight to their owner and every panel told, unknown padds refused, range clamping, rejected changes, `controlChanged()` and `control<C>()`, and a running padd's controls refused from another thread; and an owner's own changes: `setControl()` (by the same rules, telling panels), `addControl()`, `removeControl()` (the last removal empties the panel), and a padd without controls adding one |
| `audio:AudioCore_test` | The audio core, through `OfflineOutput`: the queues (including two threads at once), exact samples, voices adding up, volumes, balance, pitch, loops, stops and volume changes ramping, the limiter holding sounds that add up past full scale to it and then recovering, finished voices reported once, unloading a sound while it plays, the voice limit (refusing, and stealing the quietest or oldest, never music), files resampled from another rate and channel count, missing files, music streamed frame for frame and looping without a gap, music running dry counted, mixing allocating nothing (and that an allocation there would be counted), and how long a second of 32 voices takes to mix |
| `audio:MadeSounds_test` | Sounds made in code: their lengths at two rates, peaking at 0.9, starting and ending without a click, and the same every time |
| `audio:Midi_test` | MIDI: events' bytes (made, and read from a port's bytes), MIDI files read with their times in frames, the built-in instrument (pitch by key, notes held or dying away, sustain pedal, pitch bend, volume and pan, drums, all-sound-off without a click, taking over notes when full), a SoundFont made in code played in tune, either instrument and the engine playing without allocating, sequences on time, looping until stopped, one at a time, choosing the instrument, and how long a second of 32 notes takes |
| `audio:AudioPadd_test` | AudioPadd in a running system: answers to loading and playing, `VoiceFinished` to the padd that played, failures answered with why, padds that don't listen for answers, the volume controls and mute, stopping one voice, music or everything, four padds asking at once while the audio thread mixes, MIDI files and events, `StopMusic` stopping MIDI, and loading SoundFonts |
| `midi_input:MidiInputPadd_test` | With a virtual MIDI port as the keyboard (and `portFilter()` listening to it alone): a note reaching listeners with its port and playing on AudioPadd's instrument, `playThrough` off, and a keyboard unplugged (skipped without a MIDI system, as in Linux containers). Under ThreadSanitizer it uses `tests/tsan_suppressions.txt`, which hides races TSan reports inside RtMidi's Core MIDI callback because it can't see Core MIDI's own ordering |
| `graphics:Theme_test` | The compiled-in fonts are TrueType fonts; the theme sets the text and title fonts (different ones), the spacing, and the dark and light palettes |
| `utilities:EventTimer_test` | Repeating and one-shot timers, stopping |

## Known Limitations

- **Dispatch is a runtime lookup.** Unlike `std::visit`, nothing checks at compile time that a type is handled, since handlers are registered while the program runs (and can change). A broadcast nobody handles is dropped, with a warning the first time for its type; a direct send to a padd without a handler is refused, with an error.
- **Audio:** on the 3DS the engine is used directly (no padds there yet), and sound has been heard only in the Azahar emulator, not on a console. Sounds are held fully decoded in memory (fine for effects; stream long pieces with `PlayMusic`), and so is a SoundFont. The built-in instrument is simple, and ignores MIDI's pitch-bend range setting (RPN 0).

## Roadmap

1. Padds on the 3DS, so AudioPadd and the rest run there as on desktops.
2. **The Nintendo DS, with devkitPro's libnds** (devkitARM, the toolchain the 3DS builds already use). libnds has no threads, so every padd runs on the main thread there (see *Platforms without threads*); the rest, in order:
   1. A first `.nds` build, with the [melonDS](https://melonds.kuribo64.net) emulator run by Bazel as Azahar is for the 3DS.
   2. ScratchPadd built without threads: its locks and atomics through names that do nothing with one thread, `repeat()` run by the main loop rather than timer threads (waiting for the screen's next refresh in between), logging without spdlog (whose registry needs `std::mutex`), and a clock from the DS's hardware timers if `std::chrono::steady_clock` has none. The start and stop signalling should go through one helper, rather than an `#if` at each place it happens. Size matters too, with 4 MB of RAM for code and data.
   3. Cyclone on the DS, measured: the DS has no floating-point hardware, so `float` is done in software. A fixed-point `real` would suit it (Cyclone's `real` can already change, as the flex build shows), with the DS's hardware divide and square root.
   4. Sound through the DS's 16 hardware channels (an `Output` that gives each voice a channel rather than mixing in software, which is too slow there), and a simpler built-in instrument or Maxmod for music.

   BlocksDS, a newer SDK built on libnds, adds cooperative threads (`cothread`); if they turn out to matter, step 2 could use them instead.
