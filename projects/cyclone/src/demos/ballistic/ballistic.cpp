/*
 * Ballistic: particle projectiles.
 *
 * Each shot is one cyclone::Particle, integrated on its own every frame:
 * there are no forces, links or contacts here. The four kinds of shot
 * differ only in the particle's mass, launch velocity, constant
 * acceleration (standing in for gravity) and damping, all set in fire().
 * update() integrates the live shots and retires any that hit the ground,
 * fly past 200 m or live 5 s; display() draws the range and each shot with
 * its shadow. SAKBox: each shot sounds as it's fired (a gunshot, a cannon,
 * a whoosh of fire, a zap), and artillery booms where it lands.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include "../app.h"
#include "../ogl_headers.h"
#include "../timing.h"
#include <cyclone/cyclone.h>

#include <algorithm>
#include <stdio.h>

/**
 * The main demo class definition.
 */
class BallisticDemo : public Application {
  enum ShotType { UNUSED = 0, PISTOL, ARTILLERY, FIREBALL, LASER };

  /**
   * Holds a single ammunition round record.
   */
  struct AmmoRound {
    cyclone::Particle particle;
    ShotType type;
    unsigned startTime;

    /** Draws the round, `fraction` of the way from its last step to now. */
    void render(cyclone::real fraction) {
      const cyclone::Vector3 position = particle.getBlendedPosition(fraction);

      glColor3f(0, 0, 0);
      glPushMatrix();
      glTranslatef(position.x, position.y, position.z);
      solidSphere(0.3f, 5, 4);
      glPopMatrix();

      glColor3f(0.75, 0.75, 0.75);
      glPushMatrix();
      glTranslatef(position.x, 0, position.z);
      glScalef(1.0f, 0.1f, 1.0f);
      solidSphere(0.6f, 5, 4);
      glPopMatrix();
    }
  };

  /**
   * Holds the maximum number of  rounds that can be
   * fired.
   */
  const static unsigned ammoRounds = 16;

  /** Holds the particle data. */
  AmmoRound ammo[ammoRounds];

  /** Holds the current shot type. */
  ShotType currentShotType;

  /** Dispatches a round. */
  void fire();

public:
  /** Creates a new demo object. */
  BallisticDemo();

  /** Returns the window title for the demo. */
  const char *getTitle() override;

  /** SAKBox: the overlay explaining the demo (H hides it). */
  const char *getDescription() override {
    return "Particle projectiles: each shot is a single particle with its own "
           "mass,\n"
           "launch velocity, constant acceleration and damping.\n"
           "Pistol: fast, light gravity.   Artillery: heavy, high arc.\n"
           "Fireball: slow, floats upward.   Laser: very fast, no gravity.\n"
           "Click: fire   1-4: choose the shot";
  }

  /** Update the particle positions. */
  void update() override;

  /** SAKBox: one fixed step of `duration` (update runs those due) */
  void step(cyclone::real_duration duration);

  /** Display the particle positions. */
  void display() override;

  /** Handle a mouse click. */
  void mouse(int button, int state, int x, int y) override;

  /** Handle a keypress. */
  void key(unsigned char key) override;
};

// Method definitions
BallisticDemo::BallisticDemo() : currentShotType(LASER) {
  // Make all shots unused
  for (AmmoRound *shot = ammo; shot < ammo + ammoRounds; shot++) {
    shot->type = UNUSED;
  }
}

const char *BallisticDemo::getTitle() { return "Cyclone > Ballistic Demo"; }

void BallisticDemo::fire() {
  // Find the first available round.
  AmmoRound *shot;
  for (shot = ammo; shot < ammo + ammoRounds; shot++) {
    if (shot->type == UNUSED)
      break;
  }

  // If we didn't find a round, then exit - we can't fire.
  if (shot >= ammo + ammoRounds)
    return;

  // Set the properties of the particle
  switch (currentShotType) {
  case PISTOL:
    shot->particle.setMass(2.0f);                  // 2.0kg
    shot->particle.setVelocity(0.0f, 0.0f, 35.0f); // 35m/s
    shot->particle.setAcceleration(0.0f, -1.0f, 0.0f);
    shot->particle.setDamping(0.99f);
    break;

  case ARTILLERY:
    shot->particle.setMass(200.0f);                 // 200.0kg
    shot->particle.setVelocity(0.0f, 30.0f, 40.0f); // 50m/s
    shot->particle.setAcceleration(0.0f, -20.0f, 0.0f);
    shot->particle.setDamping(0.99f);
    break;

  case FIREBALL:
    shot->particle.setMass(1.0f);                     // 1.0kg - mostly blast damage
    shot->particle.setVelocity(0.0f, 0.0f, 10.0f);    // 5m/s
    shot->particle.setAcceleration(0.0f, 0.6f, 0.0f); // Floats up
    shot->particle.setDamping(0.9f);
    break;

  case LASER:
    // Note that this is the kind of laser bolt seen in films,
    // not a realistic laser beam!
    shot->particle.setMass(0.1f);                     // 0.1kg - almost no weight
    shot->particle.setVelocity(0.0f, 0.0f, 100.0f);   // 100m/s
    shot->particle.setAcceleration(0.0f, 0.0f, 0.0f); // No gravity
    shot->particle.setDamping(0.99f);
    break;

  case UNUSED: // an empty ammo slot, never the selected type
    break;
  }

  // SAKBox: the sound of firing it, from the gun (left of the middle: the
  // camera looks across the range, which runs away to the right)
  const float gunPan = panAcross(-0.4f);
  switch (currentShotType) {
  case PISTOL:
    sounds.play("shot", {.volume = 0.8f, .pan = gunPan, .pitch = 1.2f});
    break;
  case ARTILLERY:
    sounds.play("shot", {.volume = 1.0f, .pan = gunPan, .pitch = 0.5f}); // a big gun: lower
    break;
  case FIREBALL:
    sounds.play("whoosh", {.volume = 0.8f, .pan = gunPan, .pitch = 0.7f});
    break;
  case LASER:
    sounds.play("zap", {.volume = 0.7f, .pan = gunPan});
    break;
  case UNUSED:
    break;
  }

  // Set the data common to all particle types
  shot->particle.setPosition(0.0f, 1.5f, 0.0f);
  shot->startTime = TimingData::get().lastFrameTimestamp;
  shot->type = currentShotType;

  // Clear the force accumulators
  shot->particle.clearAccumulator();
  // SAKBox: drawn from the gun, not sliding from where the round last was
  shot->particle.rememberPosition();
}

void BallisticDemo::update() {
  // SAKBox: in fixed steps (see FixedSteps), where it ran a step as long as
  // each frame
  runDueSteps([this](cyclone::real_duration seconds) { step(seconds); });
  Application::update();
}

void BallisticDemo::step(cyclone::real_duration duration) {
  // Update the physics of each particle in turn
  for (AmmoRound *shot = ammo; shot < ammo + ammoRounds; shot++) {
    if (shot->type != UNUSED) {
      // Run the physics, drawn between where it was and where it gets to
      shot->particle.rememberPosition();
      shot->particle.integrate(duration);

      // SAKBox: artillery booms where it lands, quieter the farther off
      if (shot->type == ARTILLERY && shot->particle.getPosition().y < 0.0f) {
        const float distance = static_cast<float>(shot->particle.getPosition().z);
        sounds.play("blast", {.volume = std::clamp(20.0f / distance, 0.2f, 1.0f),
                              .pan = panAcross((distance - 22.0f) / 80.0f)});
      }

      // Check if the particle is now invalid
      if (shot->particle.getPosition().y < 0.0f ||
          shot->startTime + 5000 < TimingData::get().lastFrameTimestamp ||
          shot->particle.getPosition().z > 200.0f) {
        // We simply set the shot type to be unused, so the
        // memory it occupies can be reused by another shot.
        shot->type = UNUSED;
      }
    }
  }
}

void BallisticDemo::display() {
  // Clear the viewport and set the camera direction
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(-25.0, 8.0, 5.0, 0.0, 5.0, 22.0, 0.0, 1.0, 0.0);

  // Draw a sphere at the firing point, and add a shadow projected
  // onto the ground plane.
  glColor3f(0.0f, 0.0f, 0.0f);
  glPushMatrix();
  glTranslatef(0.0f, 1.5f, 0.0f);
  solidSphere(0.1f, 5, 5);
  glTranslatef(0.0f, -1.5f, 0.0f);
  glColor3f(0.75f, 0.75f, 0.75f);
  glScalef(1.0f, 0.1f, 1.0f);
  solidSphere(0.1f, 5, 5);
  glPopMatrix();

  // Draw some scale lines
  glColor3f(0.75f, 0.75f, 0.75f);
  glBegin(GL_LINES);
  for (unsigned i = 0; i < 200; i += 10) {
    glVertex3f(-5.0f, 0.0f, i);
    glVertex3f(5.0f, 0.0f, i);
  }
  glEnd();

  // Render each particle in turn
  for (AmmoRound *shot = ammo; shot < ammo + ammoRounds; shot++) {
    if (shot->type != UNUSED) {
      shot->render(drawFraction());
    }
  }

  // Render the description
  glColor3f(0.0f, 0.0f, 0.0f);
  renderText(10.0f, 34.0f, "Click: Fire\n1-4: Select Ammo");

  // Render the name of the current shot type
  switch (currentShotType) {
  case PISTOL:
    renderText(10.0f, 10.0f, "Current Ammo: Pistol");
    break;
  case ARTILLERY:
    renderText(10.0f, 10.0f, "Current Ammo: Artillery");
    break;
  case FIREBALL:
    renderText(10.0f, 10.0f, "Current Ammo: Fireball");
    break;
  case LASER:
    renderText(10.0f, 10.0f, "Current Ammo: Laser");
    break;
  case UNUSED:
    break;
  }
}

void BallisticDemo::mouse(int button, int state, int x, int y) {
  // Fire the current weapon.
  if (state == MOUSE_DOWN)
    fire();
}

void BallisticDemo::key(unsigned char key) {
  switch (key) {
  case '1':
    currentShotType = PISTOL;
    break;
  case '2':
    currentShotType = ARTILLERY;
    break;
  case '3':
    currentShotType = FIREBALL;
    break;
  case '4':
    currentShotType = LASER;
    break;
  }
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application *getApplication() { return new BallisticDemo(); }