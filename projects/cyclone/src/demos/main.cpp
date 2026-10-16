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

// Forward declaration of the function that will return the
// application object for this particular demo. This should be
// implemented in the demo's .cpp file.
extern Application *getApplication();

// Store the global application object.
Application *app;

/**
 * Creates a window in which to display the scene.
 */
void createWindow(const char *title) {
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(640, 320);
  glutInitWindowPosition(0, 0);
  glutCreateWindow(title);
}

/**
 * Called each frame to update the 3D scene. Delegates to
 * the application.
 */
void update() {
  // Update the timing.
  TimingData::get().update();

  // Delegate to the application.
  app->update();
}

/**
 * Called each frame to display the 3D scene. Delegates to
 * the application.
 */
void display() {
  app->display();

  // Update the displayed content.
  glFlush();
  glutSwapBuffers();
}

/**
 * Called when a mouse button is pressed. Delegates to the
 * application.
 */
void mouse(int button, int state, int x, int y) {
  app->mouse(button, state, x, y);
}

/**
 * Called when the display window changes size.
 */
void reshape(int width, int height) { app->resize(width, height); }

/**
 * Called when a key is pressed.
 */
void keyboard(unsigned char key, int x, int y) {
  // Note we omit passing on the x and y: they are rarely needed.
  app->key(key);
}

/**
 * Called when the mouse is dragged.
 */
void motion(int x, int y) { app->mouseDrag(x, y); }

// SAKBox: --keys "SECONDS:INPUT,..." plays input into the demo, for testing
// its controls without touching the keyboard. At each time (seconds after
// launch) every character of INPUT goes to the demo's key handler in turn,
// or INPUT "click" is a left click. E.g. --keys "1:2,1.5:click,3:r".
struct ScriptedInput {
  int atMs;
  std::string input;
};
static std::vector<ScriptedInput> scriptedInput;

static void playScriptedInput(int index) {
  const std::string &input = scriptedInput[index].input;
  if (input == "click") {
    app->mouse(GLUT_LEFT_BUTTON, GLUT_DOWN, 0, 0);
    app->mouse(GLUT_LEFT_BUTTON, GLUT_UP, 0, 0);
  } else {
    for (char key : input)
      app->key(static_cast<unsigned char>(key));
  }
  std::printf("scripted input at %.2f s: %s\n",
              scriptedInput[index].atMs / 1000.0, input.c_str());
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
    scriptedInput.push_back(
        {static_cast<int>(seconds * 1000), entry.substr(colon + 1)});
  }
  return true;
}

/**
 * The main entry point. We pass arguments onto GLUT. SAKBox: `--run` starts
 * demos that normally begin paused (the rigid-body demos) already running.
 */
int main(int argc, char **argv) {
  // Set up GLUT and the timers
  glutInit(&argc, argv);
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
  createWindow(app->getTitle());

  // Set up the appropriate handler functions
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);
  glutDisplayFunc(display);
  glutIdleFunc(update);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);

  for (size_t i = 0; i < scriptedInput.size(); ++i)
    glutTimerFunc(scriptedInput[i].atMs, playScriptedInput,
                  static_cast<int>(i));

  // Run the application
  app->initGraphics();
  glutMainLoop();

  // Clean up the application
  app->deinit();
  delete app;
  TimingData::deinit();
}
