/*
 * The main entry point for all demos.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

// Include appropriate OpenGL headers.
#include "ogl_headers.h"

// Include the general application structure.
#include "app.h"

// Include the timing functions
#include "timing.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// SAKBox: the demos run in a GLFW window; they originally used GLUT. GLFW
// calls the functions below, which pass events to the demo, as GLUT's
// callbacks did.

// Forward declaration of the function that will return the
// application object for this particular demo. This should be
// implemented in the demo's .cpp file.
extern Application *getApplication();

// Store the global application object.
Application *app;

// Whether a mouse button is held, so moves are reported as drags
static bool mouseHeld = false;

static void framebufferResized(GLFWwindow *window, int width, int height) {
  int pointsWide = 0, pointsHigh = 0;
  glfwGetWindowSize(window, &pointsWide, &pointsHigh);
  app->setPixelScale(pointsWide > 0 ? (float)width / pointsWide : 1.0f);
  app->resize(width, height);
}

static void typed(GLFWwindow *, unsigned int codepoint) {
  // H shows or hides the description overlay; no demo uses it. The demos'
  // keys are all ASCII.
  if (codepoint == 'h' || codepoint == 'H')
    app->toggleOverlay();
  else if (codepoint < 128)
    app->key(static_cast<unsigned char>(codepoint));
}

static void mouseButton(GLFWwindow *window, int button, int action, int) {
  int demoButton = button == GLFW_MOUSE_BUTTON_RIGHT    ? MOUSE_RIGHT_BUTTON
                   : button == GLFW_MOUSE_BUTTON_MIDDLE ? MOUSE_MIDDLE_BUTTON
                                                        : MOUSE_LEFT_BUTTON;
  mouseHeld = action == GLFW_PRESS;
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  app->mouse(demoButton, mouseHeld ? MOUSE_DOWN : MOUSE_UP, (int)x, (int)y);
}

static void mouseMoved(GLFWwindow *, double x, double y) {
  if (mouseHeld)
    app->mouseDrag((int)x, (int)y);
}

// SAKBox: --keys "SECONDS:INPUT,..." plays input into the demo, for testing
// its controls without touching the keyboard. At each time (seconds after
// launch) every character of INPUT goes to the demo's key handler in turn,
// or INPUT "click" is a left click. E.g. --keys "1:2,1.5:click,3:r".
struct ScriptedInput {
  double atSeconds;
  std::string input;
};
static std::vector<ScriptedInput> scriptedInput;

static void playScriptedInput(const ScriptedInput &scripted) {
  if (scripted.input == "click") {
    app->mouse(MOUSE_LEFT_BUTTON, MOUSE_DOWN, 0, 0);
    app->mouse(MOUSE_LEFT_BUTTON, MOUSE_UP, 0, 0);
  } else {
    // Through typed(), as real key presses are (so H works here too)
    for (char key : scripted.input)
      typed(nullptr, static_cast<unsigned char>(key));
  }
  std::printf("scripted input at %.2f s: %s\n", scripted.atSeconds,
              scripted.input.c_str());
  std::fflush(stdout);
}

static bool parseScriptedInput(const char *script) {
  std::string rest = script;
  while (!rest.empty()) {
    const size_t comma = rest.find(',');
    const std::string entry = rest.substr(0, comma);
    rest = comma == std::string::npos ? "" : rest.substr(comma + 1);
    const size_t colon = entry.find(':');
    if (colon == std::string::npos || colon + 1 == entry.size())
      return false;
    char *end = nullptr;
    const double seconds = std::strtod(entry.c_str(), &end);
    if (end != entry.c_str() + colon || seconds < 0)
      return false;
    scriptedInput.push_back({seconds, entry.substr(colon + 1)});
  }
  return true;
}

int main(int argc, char **argv) {
  if (!glfwInit()) {
    std::fprintf(stderr, "Couldn't start GLFW\n");
    return 1;
  }
  TimingData::init();

  // Create the application and its window
  app = getApplication();
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--run") == 0) {
      app->startRunning();
    } else if (std::strcmp(argv[i], "--keys") == 0 && i + 1 < argc) {
      if (!parseScriptedInput(argv[++i])) {
        std::fprintf(stderr, "--keys wants \"SECONDS:INPUT,...\", got %s\n",
                     argv[i]);
        return 1;
      }
    }
  }

  // The default context: legacy OpenGL, which the demos draw with
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  GLFWwindow *window =
      glfwCreateWindow(640, 320, app->getTitle(), nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "Couldn't create a window\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  // Set up the appropriate handler functions
  glfwSetFramebufferSizeCallback(window, framebufferResized);
  glfwSetCharCallback(window, typed);
  glfwSetMouseButtonCallback(window, mouseButton);
  glfwSetCursorPosCallback(window, mouseMoved);

  // Run the application
  int width = 0, height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  framebufferResized(window, width, height);
  app->initGraphics();
  const double start = glfwGetTime();
  size_t nextScripted = 0;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    while (nextScripted < scriptedInput.size() &&
           glfwGetTime() - start >= scriptedInput[nextScripted].atSeconds)
      playScriptedInput(scriptedInput[nextScripted++]);

    // Update the timing, then the demo, then draw it
    TimingData::get().update();
    app->update();
    app->display();
    app->renderOverlay();
    glFlush();
    glfwSwapBuffers(window);
  }

  // Clean up the application
  app->deinit();
  delete app;
  TimingData::deinit();
  glfwDestroyWindow(window);
  glfwTerminate();
}
