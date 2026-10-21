/*
 * The base application class for all demos.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */
#include <cstdlib>

#include <cyclone/cyclone.h>

/**
 * An application is the base class for all demonstration progams.
 * The windowing API (GLUT originally; SAKBox uses GLFW) calls bare
 * functions. This makes it more difficult to provide default services
 * for all demos and only override them when needed.
 *
 * To solve this, the windowing API is translated into calls on a
 * generic application object. Each demonstration will create a
 * concrete subclass of Application, providing the behaviours it
 * needs. The common code for all demos manages dispatch of
 * requests to the appropriate application object.
 *
 * To provide a correct application object of the right type without
 * the core code needing to know which subclass is being used, each
 * demonstration will supply a getApplication function which creates
 * (with new) and returns a pointer to a new Application instance.
 *
 * Even though subclasses will have to implement most of the methods
 * in this class, I have not made them pure virtual. This saves the
 * annoying need to implement an empty function that isn't needed.
 */
class Application {
public:
  // Virtual: main.cpp deletes the demo through an Application pointer.
  virtual ~Application() = default;

  // SAKBox: called for the --run command-line option. Demos that start
  // paused begin running instead; the others ignore it.
  virtual void startRunning() {}

protected:
  /**
   * Holds the height of the application window, in pixels.
   */
  int height = 320;

  /**
   * Holds the current width of the application window, in pixels.
   */
  int width = 640;

  /**
   * SAKBox: pixels per point (2 on a Retina display). Text is drawn this
   * much larger so it keeps its size on screen.
   */
  float pixelScale = 1.0f;

  /** SAKBox: whether the description overlay is showing. */
  bool showOverlay = true;

public:
  // SAKBox: set by main.cpp from the window's framebuffer and point sizes.
  void setPixelScale(float scale) { pixelScale = scale; }

public:
  /**
   * Gets the title of the demo for the title bar of the window.
   *
   * The default implementation returns a generic title.
   */
  virtual const char *getTitle();

  /**
   * SAKBox: a few lines explaining what the demo shows and how to drive
   * it, drawn over the top left of the window (H hides it). Lines are
   * separated by '\n'. The default, nullptr, shows nothing.
   */
  virtual const char *getDescription() { return nullptr; }

  /** SAKBox: shows or hides the description overlay (the H key). */
  void toggleOverlay() { showOverlay = !showOverlay; }

  /**
   * SAKBox: draws the description over the scene. main.cpp calls it after
   * display() each frame.
   */
  void renderOverlay();

  /**
   * Sets up the graphics, and allows the application to acquire
   * graphical resources. Guaranteed to be called after OpenGL is
   * set up.
   *
   * The default implementation sets up a basic view, and calls
   * setView to set up the camera projection.
   */
  virtual void initGraphics();

  /**
   * Called to set the projection characteristics of the camera.
   *
   * The default implementation uses a 60 degree field of view camera
   * with a range from 1-500 units.
   */
  virtual void setView();

  /**
   * Called just before the application is destroyed. Clear up can
   * be performed here or in the application destructor.
   *
   * The default implementation does nothing.
   */
  virtual void deinit();

  /**
   * Called each frame to display the current scene. The common code
   * will automatically flush the graphics pipe and swap the render
   * buffers after calling this so glFlush doesn't need to be called.
   *
   * The default
   * implementation draws a simple diagonal line across the surface
   * (as a sanity check to make sure GL is working).
   */
  virtual void display();

  /**
   * Called each frame to update the current state of the scene.
   *
   * The default implementation requests that the display be refreshed.
   * It should probably be called from any subclass update as the last
   * command.
   */
  virtual void update();

  /**
   * Called when a keypress is detected.
   *
   * The default implementation does nothing.
   *
   * @param key The ascii code of the key that has been pressed.
   */
  virtual void key(unsigned char key);

  /**
   * Notifies the application that the window has changed size.
   * The new size is given.
   *
   * The default implementation sets the internal height and width
   * parameters and changes the gl viewport. These are steps you'll
   * almost always need, so its worth calling the base class version
   * of this method even if you override it in a demo class.
   */
  virtual void resize(int width, int height);

  /**
   * Called when a mouse button is pressed or released (button is one of
   * the MOUSE_*_BUTTON values and state MOUSE_DOWN or MOUSE_UP, from
   * ogl_headers.h; x and y are in window points).
   *
   * The default implementation does nothing.
   */
  virtual void mouse(int button, int state, int x, int y);

  /**
   * Called when the mouse moves with a button held.
   *
   * The default implementation does nothing.
   */
  virtual void mouseDrag(int x, int y);

  // These are helper functions that can be used by an application
  // to render things.

  /**
   * Renders the given text to the given x,y location (in screen space)
   * on the window, in points from the bottom left. This is used to pass
   * status information to the application.
   */
  void renderText(float x, float y, const char *text);
};

/**
 * This application adds additional functionality used in the mass-aggregate
 * demos.
 */
class MassAggregateApplication : public Application {
protected:
  cyclone::ParticleWorld world;
  cyclone::Particle *particleArray;
  cyclone::GroundContacts groundContactGenerator;

public:
  MassAggregateApplication(unsigned int particleCount);
  ~MassAggregateApplication() override;

  /** Update the particle positions. */
  void update() override;

  /** Sets up the graphic rendering. */
  void initGraphics() override;

  /** Display the particles. */
  void display() override;
};

// SAKBox: the keys every RigidBodyApplication handles, for the demos'
// overlays (see Application::getDescription)
#define RIGID_BODY_KEYS                                                                            \
  "P: run/pause   Space: one frame   R: reset   C: contacts   Drag: camera\n"                      \
  "M: switch contact resolver (shown bottom right)"

/**
 * This application adds additional functionality used in many of the
 * demos. This includes the ability to track contacts (for rigid bodies)
 * and move the camera around.
 */
class RigidBodyApplication : public Application {
protected:
  /** Holds the maximum number of contacts. */
  const static unsigned maxContacts = 256;

  /** Holds the array of contacts. */
  cyclone::Contact contacts[maxContacts];

  /** Holds the collision data structure for collision detection. */
  cyclone::CollisionData cData;

  /** Holds the contact resolver. */
  cyclone::ContactResolver resolver;

  /** Holds the camera angle. */
  float theta;

  /** Holds the camera elevation. */
  float phi;

  /** Holds the position of the mouse at the last frame of a drag. */
  int last_x, last_y;

  /** True if the contacts should be rendered. */
  bool renderDebugInfo;

  /** True if the simulation is paused. */
  bool pauseSimulation;

  /** Pauses the simulation after the next frame automatically */
  bool autoPauseSimulation;

  /** Processes the contact generation code. */
  virtual void generateContacts() = 0;

  /** Processes the objects in the simulation forward in time. */
  virtual void updateObjects(cyclone::real duration) = 0;

  /**
   * SAKBox: writes which contact resolver is in use in the bottom right
   * corner (M switches; see cyclone::ContactResolver::Method). drawDebug()
   * calls it.
   */
  void renderResolverMethod();

  /**
   * Finishes drawing the frame, adding debugging information
   * as needed.
   */
  void drawDebug();

  /** Resets the simulation. */
  virtual void reset() = 0;

public:
  /**
   * Creates a new application object.
   */
  RigidBodyApplication();

  // Starts with the simulation running instead of paused (--run).
  void startRunning() override {
    pauseSimulation = false;
    autoPauseSimulation = false;
  }

  /** Display the application. */
  void display() override;

  /** Update the objects. */
  void update() override;

  /** Handle a mouse click. */
  void mouse(int button, int state, int x, int y) override;

  /** Handle a mouse drag */
  void mouseDrag(int x, int y) override;

  /** Handles a key press. */
  void key(unsigned char key) override;
};
