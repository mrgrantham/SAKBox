// SAKBox: the window the demos run in (see main.cpp, and window.cpp for its
// options: --run, --keys).
#ifndef CYCLONE_DEMOS_WINDOW_H
#define CYCLONE_DEMOS_WINDOW_H

#include <functional>

class Application;

// Opens a GLFW window for the application `makeApplication` makes (with new,
// once the window system and timing are started) and runs it until the
// window is closed, then deletes it. Takes the demos' command-line options from
// argv[1] on: --run starts it running, --keys "SECONDS:INPUT,..." plays
// input into it. Returns main's exit code.
int runInWindow(const std::function<Application *()> &makeApplication, int argc, char **argv);

#endif // CYCLONE_DEMOS_WINDOW_H
