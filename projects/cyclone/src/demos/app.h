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
#include <algorithm>
#include <cstdlib>
#include <functional>

#include "demo_sounds.h"
#include "impact_sounds.h"
#include <ScratchPadd/Audio/DeviceOutput.hpp>
#include <cyclone/cyclone.h>
#include <fixed_steps.h>
#include <memory>
#include <span>
#include <string>

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

  /**
   * SAKBox: the demo's sounds (see DemoSounds), through the sound device.
   * V turns them off and on; the bottom right says which.
   */
  ScratchPadd::Audio::DeviceOutput soundOutput;
  DemoSounds sounds{soundOutput};

  /**
   * SAKBox: knocks when things hit (see ImpactSounds): a demo has it hear
   * its contacts, and update() plays the knocks each frame.
   */
  ImpactSounds impacts{sounds};

  /** SAKBox: has the knocks hear `world`'s contacts each step. */
  void hearContacts(cyclone::ParticleWorld &world) {
    world.contactsFound = [this](std::span<const cyclone::ParticleContact> contacts,
                                 cyclone::real_duration seconds) {
      impacts.hear(contacts, static_cast<float>(seconds));
    };
  }

  /**
   * SAKBox: how high above the bottom right the sound's state is written
   * (points), clear of what the demo writes there
   */
  float soundStateHeight = 10.0f;

  /** SAKBox: the sound's state, for the bottom right. */
  virtual std::string soundState();

  /**
   * SAKBox: physics runs in fixed steps of physicsClock's length, whatever
   * the frame rate (see FixedSteps), each in its sub-steps in demos whose
   * world takes them (4 to start, FixedSteps::kDefaultSubSteps; [ and ]
   * change how many)
   */
  FixedSteps physicsClock;

  /** SAKBox: milliseconds of physics a frame, smoothed (for the corner) */
  double physicsMilliseconds = 0;

  /**
   * SAKBox: whether bodies are drawn blended between their last two steps
   * (B turns it off and on), so they move smoothly on screens whose frames
   * don't fall on the steps, one step behind
   */
  bool blendDrawing = true;

  /** SAKBox: whether the simulation is paused (it isn't, unless a demo says) */
  virtual bool simulationPaused() const { return false; }

  /**
   * SAKBox: how far between their last step and now to draw the bodies
   * and particles (cyclone::RigidBody::getBlendedGLTransform,
   * cyclone::Particle::getBlendedPosition): how far the frame is into the
   * next step, or 1 (where they are) when paused or not blending
   */
  cyclone::real drawFraction() const {
    return simulationPaused() || !blendDrawing ? (cyclone::real)1
                                               : (cyclone::real)physicsClock.stepFraction();
  }

  /**
   * SAKBox: runs the fixed steps the last frame covers (physicsClock),
   * calling `step` with each one's length, and times them for the corner
   */
  void runDueSteps(const std::function<void(cyclone::real_duration)> &step);

  /**
   * SAKBox: writes the steps a second, the sub-steps, whether drawing is
   * blended and the time physics takes a frame, above the sound
   */
  void renderPhysicsSteps();

  /** SAKBox: pan (-0.8 left to 0.8 right) for something `across` of the way across the screen (-1
   * to 1). */
  static float panAcross(float across) { return 0.8f * std::clamp(across, -1.0f, 1.0f); }

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

  /** SAKBox: turns sound off or on (the V key). */
  void toggleSound() { sounds.setEnabled(!sounds.enabled()); }

  /** SAKBox: whether the demo's world takes sub-steps ([ and ] set them) */
  virtual bool usesSubSteps() const { return false; }

  /** SAKBox: turns drawing blended between steps off or on (the B key). */
  void toggleBlending() { blendDrawing = !blendDrawing; }

  /**
   * SAKBox: takes `change` more sub-steps a step, if the demo's world takes
   * them (the [ and ] keys)
   */
  void changeSubSteps(int change) {
    if (usesSubSteps())
      physicsClock.setSubSteps(physicsClock.subSteps() + change);
  }

  /**
   * SAKBox: draws the description over the scene, and the sound's state
   * in the bottom right. main.cpp calls it after display() each frame.
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
   * command. (SAKBox: it plays the knocks heard, and collects the sounds
   * that have finished.)
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

  /**
   * SAKBox: renders `text` on a translucent white panel whose top left is
   * at `left, top` (points from the bottom left), with `margin` around it,
   * so it reads on light and dark scenes
   */
  void renderPanel(float left, float top, float margin, const std::string &text);
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

  /** SAKBox: the particle world takes sub-steps */
  bool usesSubSteps() const override { return true; }

  /** Sets up the graphic rendering. */
  void initGraphics() override;

  /** Display the particles. */
  void display() override;
};

// SAKBox: the keys every RigidBodyApplication handles, for the demos'
// overlays (see Application::getDescription)
#define RIGID_BODY_KEYS                                                                            \
  "P: run/pause   Space: one step   R: reset   C: contacts   Drag: camera\n"                       \
  "M: switch contact resolver (shown bottom right)"

/**
 * This application adds additional functionality used in many of the
 * demos. This includes the ability to track contacts (for rigid bodies)
 * and move the camera around.
 */
class RigidBodyApplication : public Application {
protected:
  /**
   * SAKBox: the world the demo simulates. The demo adds its bodies, shapes,
   * planes and joints to it; runStep() steps it, the sounds hear its
   * contacts and drawDebug() draws them.
   */
  virtual cyclone::World &getWorld() = 0;

  /**
   * SAKBox: the settings the demos' worlds share: room for 256 contacts, the
   * friction they've always had, how bouncy the demo's bodies are, and as
   * many iterations of the book's resolver as they've always had
   */
  static cyclone::World::Settings demoWorldSettings(cyclone::real restitution) {
    constexpr unsigned maxContacts = 256;
    return {.maxContacts = maxContacts,
            .friction = (cyclone::real)0.9,
            .restitution = restitution,
            .iterations = maxContacts * 8};
  }

  /** SAKBox: the ground (y = 0), for a world to add. */
  static cyclone::CollisionPlane ground() {
    cyclone::CollisionPlane plane;
    plane.direction = cyclone::Vector3(0, 1, 0);
    plane.offset = 0;
    return plane;
  }

  /** Holds the camera angle. */
  float theta;

  /** Holds the camera elevation. */
  float phi;

  /**
   * SAKBox: how far the camera is from what it looks at, and how high that
   * is (m): the demos' scenes are about 10 m across; a smaller one comes
   * closer.
   */
  float cameraDistance = 18.0f;
  float cameraTargetHeight = 5.0f;

  /** Holds the position of the mouse at the last frame of a drag. */
  int last_x, last_y;

  /** SAKBox: the sound's state, with how many knocks have played. */
  std::string soundState() override;

  /** True if the contacts should be rendered. */
  bool renderDebugInfo;

  /** True if the simulation is paused. */
  bool pauseSimulation;

  /** Pauses the simulation after the next frame automatically */
  bool autoPauseSimulation;

  /** SAKBox: paused with P, and while stepping once with Space */
  bool simulationPaused() const override { return pauseSimulation; }

  /** SAKBox: the world takes sub-steps */
  bool usesSubSteps() const override { return true; }

  /**
   * SAKBox: called before each step, after its forces are cleared (forces
   * added here act through every sub-step), and after it. Do nothing by
   * default.
   */
  virtual void beforeStep() {}
  virtual void afterStep() {}

  /**
   * SAKBox: called each time the world finds contacts, before they're
   * resolved (see cyclone::World::contactsFound), after the sounds hear
   * them. Does nothing by default.
   */
  virtual void contactsFound(std::span<const cyclone::Contact> contacts) {}

  /** SAKBox: one physics step of the world, in its sub-steps */
  void runStep();

  /**
   * SAKBox: writes which contact resolver is in use in the bottom right
   * corner (M switches; see cyclone::ContactResolver::Method). drawDebug()
   * calls it.
   */
  void renderResolverMethod();

  /**
   * SAKBox: the bottom-right corner: the resolver and the steps. drawDebug()
   * calls it; a demo that doesn't use drawDebug() calls it itself.
   */
  void renderCornerStatus();

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
