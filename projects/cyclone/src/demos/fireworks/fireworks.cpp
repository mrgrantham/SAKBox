/*
 * Fireworks: particles that spawn more particles.
 *
 * A Firework is a particle with a type and an age (its fuse). Each frame
 * update() integrates every live firework and counts its fuse down; when
 * it burns out, the FireworkRule for its type launches its payloads: so
 * many new fireworks of given types, inheriting the parent's velocity plus
 * a random spread. initFireworkRules() defines the nine types, some of
 * which burst into others, and a slot pool holds the fireworks in flight.
 * SAKBox: a launch whooshes, and a burst in the air crackles, panned to
 * where it is.
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

#include <optional>
#include <stdio.h>

static cyclone::Random crandom;

/**
 * Fireworks are particles, with additional data for rendering and
 * evolution.
 */
class Firework : public cyclone::Particle {
public:
  /** Fireworks have an integer type, used for firework rules. */
  unsigned type;

  /**
   * The age of a firework determines when it detonates. Age gradually
   * decreases, when it passes zero the firework delivers its payload.
   * Think of age as fuse-left.
   */
  cyclone::real age;

  /**
   * Updates the firework by the given duration of time. Returns true
   * if the firework has reached the end of its life and needs to be
   * removed.
   */
  bool update(cyclone::real duration) {
    // Update our physical state
    integrate(duration);

    // We work backwards from our age to zero.
    age -= duration;
    return (age < 0) || (position.y < 0);
  }
};

/**
 * Firework rules control the length of a firework's fuse and the
 * particles it should evolve into.
 */
struct FireworkRule {
  /** The type of firework that is managed by this rule. */
  unsigned type;

  /** The minimum length of the fuse. */
  cyclone::real minAge;

  /** The maximum legnth of the fuse. */
  cyclone::real maxAge;

  /** The minimum relative velocity of this firework. */
  cyclone::Vector3 minVelocity;

  /** The maximum relative velocity of this firework. */
  cyclone::Vector3 maxVelocity;

  /** The damping of this firework type. */
  cyclone::real damping;

  /**
   * The payload is the new firework type to create when this
   * firework's fuse is over.
   */
  struct Payload {
    /** The type of the new particle to create. */
    unsigned type;

    /** The number of particles in this payload. */
    unsigned count;

    /** Sets the payload properties in one go. */
    void set(unsigned type, unsigned count) {
      Payload::type = type;
      Payload::count = count;
    }
  };

  /** The number of payloads for this firework type. */
  unsigned payloadCount;

  /** The set of payloads. */
  Payload *payloads;

  FireworkRule() : payloadCount(0), payloads(nullptr) {}

  void init(unsigned payloadCount) {
    FireworkRule::payloadCount = payloadCount;
    payloads = new Payload[payloadCount];
  }

  ~FireworkRule() {
    if (payloads != nullptr)
      delete[] payloads;
  }

  /**
   * Set all the rule parameters in one go.
   */
  void setParameters(unsigned type, cyclone::real minAge, cyclone::real maxAge,
                     const cyclone::Vector3 &minVelocity, const cyclone::Vector3 &maxVelocity,
                     cyclone::real damping) {
    FireworkRule::type = type;
    FireworkRule::minAge = minAge;
    FireworkRule::maxAge = maxAge;
    FireworkRule::minVelocity = minVelocity;
    FireworkRule::maxVelocity = maxVelocity;
    FireworkRule::damping = damping;
  }

  /**
   * Creates a new firework of this type and writes it into the given
   * instance. The optional parent firework is used to base position
   * and velocity on.
   */
  void create(Firework *firework, const Firework *parent = nullptr) const {
    firework->type = type;
    firework->age = crandom.randomReal(minAge, maxAge);

    cyclone::Vector3 vel;
    if (parent) {
      // The position and velocity are based on the parent.
      firework->setPosition(parent->getPosition());
      vel += parent->getVelocity();
    } else {
      cyclone::Vector3 start;
      int x = (int)crandom.randomInt(3) - 1;
      start.x = 5.0f * cyclone::real(x);
      firework->setPosition(start);
    }

    vel += crandom.randomVector(minVelocity, maxVelocity);
    firework->setVelocity(vel);

    // We use a mass of one in all cases (no point having fireworks
    // with different masses, since they are only under the influence
    // of gravity).
    firework->setMass(1);

    firework->setDamping(damping);

    firework->setAcceleration(cyclone::Vector3::GRAVITY);

    firework->clearAccumulator();
  }
};

/**
 * The main demo class definition.
 */
class FireworksDemo : public Application {
  /**
   * Holds the maximum number of fireworks that can be in use.
   */
  const static unsigned maxFireworks = 1024;

  /** Holds the firework data. */
  Firework fireworks[maxFireworks];

  /** Holds the index of the next firework slot to use. */
  unsigned nextFirework;

  /** And the number of rules. */
  const static unsigned ruleCount = 9;

  /** Holds the set of rules. */
  FireworkRule rules[ruleCount];

  /** Dispatches a firework from the origin. */
  void create(unsigned type, const Firework *parent = nullptr);

  /** Dispatches the given number of fireworks from the given parent. */
  void create(unsigned type, unsigned number, const Firework *parent);

  /** Creates the rules. */
  void initFireworkRules();

public:
  /** Creates a new demo object. */
  FireworksDemo();
  ~FireworksDemo() override;

  /** Sets up the graphic rendering. */
  void initGraphics() override;

  /** Returns the window title for the demo. */
  const char *getTitle() override;

  /** SAKBox: the overlay explaining the demo (H hides it). */
  const char *getDescription() override {
    return "Particles that spawn more particles: each firework is a particle "
           "with a\n"
           "fuse. When the fuse burns out, its type's rule launches a payload "
           "of new\n"
           "fireworks, which may burst again.\n"
           "1-9: launch one of the nine firework types";
  }

  /** Update the particle positions. */
  void update() override;

  /** Display the particle positions. */
  void display() override;

  /** Handle a keypress. */
  void key(unsigned char key) override;
};

// Method definitions
FireworksDemo::FireworksDemo() : nextFirework(0) {
  // Make all shots unused
  for (Firework *firework = fireworks; firework < fireworks + maxFireworks; firework++) {
    firework->type = 0;
  }

  // Create the firework types
  initFireworkRules();
}

FireworksDemo::~FireworksDemo() {}

void FireworksDemo::initFireworkRules() {
  // Go through the firework types and create their rules.
  rules[0].init(2);
  rules[0].setParameters(1,                            // type
                         0.5f, 1.4f,                   // age range
                         cyclone::Vector3(-5, 25, -5), // min velocity
                         cyclone::Vector3(5, 28, 5),   // max velocity
                         0.1                           // damping
  );
  rules[0].payloads[0].set(3, 5);
  rules[0].payloads[1].set(5, 5);

  rules[1].init(1);
  rules[1].setParameters(2,                            // type
                         0.5f, 1.0f,                   // age range
                         cyclone::Vector3(-5, 10, -5), // min velocity
                         cyclone::Vector3(5, 20, 5),   // max velocity
                         0.8                           // damping
  );
  rules[1].payloads[0].set(4, 2);

  rules[2].init(0);
  rules[2].setParameters(3,                            // type
                         0.5f, 1.5f,                   // age range
                         cyclone::Vector3(-5, -5, -5), // min velocity
                         cyclone::Vector3(5, 5, 5),    // max velocity
                         0.1                           // damping
  );

  rules[3].init(0);
  rules[3].setParameters(4,                            // type
                         0.25f, 0.5f,                  // age range
                         cyclone::Vector3(-20, 5, -5), // min velocity
                         cyclone::Vector3(20, 5, 5),   // max velocity
                         0.2                           // damping
  );

  rules[4].init(1);
  rules[4].setParameters(5,                            // type
                         0.5f, 1.0f,                   // age range
                         cyclone::Vector3(-20, 2, -5), // min velocity
                         cyclone::Vector3(20, 18, 5),  // max velocity
                         0.01                          // damping
  );
  rules[4].payloads[0].set(3, 5);

  rules[5].init(0);
  rules[5].setParameters(6,                           // type
                         3, 5,                        // age range
                         cyclone::Vector3(-5, 5, -5), // min velocity
                         cyclone::Vector3(5, 10, 5),  // max velocity
                         0.95                         // damping
  );

  rules[6].init(1);
  rules[6].setParameters(7,                            // type
                         4, 5,                         // age range
                         cyclone::Vector3(-5, 50, -5), // min velocity
                         cyclone::Vector3(5, 60, 5),   // max velocity
                         0.01                          // damping
  );
  rules[6].payloads[0].set(8, 10);

  rules[7].init(0);
  rules[7].setParameters(8,                            // type
                         0.25f, 0.5f,                  // age range
                         cyclone::Vector3(-1, -1, -1), // min velocity
                         cyclone::Vector3(1, 1, 1),    // max velocity
                         0.01                          // damping
  );

  rules[8].init(0);
  rules[8].setParameters(9,                             // type
                         3, 5,                          // age range
                         cyclone::Vector3(-15, 10, -5), // min velocity
                         cyclone::Vector3(15, 15, 5),   // max velocity
                         0.95                           // damping
  );
  // ... and so on for other firework types ...
}

void FireworksDemo::initGraphics() {
  // Call the superclass
  Application::initGraphics();

  // But override the clear color
  glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
}

const char *FireworksDemo::getTitle() { return "Cyclone > Fireworks Demo"; }

void FireworksDemo::create(unsigned type, const Firework *parent) {
  // Get the rule needed to create this firework
  FireworkRule *rule = rules + (type - 1);

  // Create the firework
  rule->create(fireworks + nextFirework, parent);

  // Increment the index for the next firework
  nextFirework = (nextFirework + 1) % maxFireworks;
}

void FireworksDemo::create(unsigned type, unsigned number, const Firework *parent) {
  for (unsigned i = 0; i < number; i++) {
    create(type, parent);
  }
}

void FireworksDemo::update() {
  // Find the duration of the last frame in seconds
  float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
  if (duration <= 0.0f)
    return;

  // SAKBox: where the highest burst this frame is, to crackle (one a frame:
  // fireworks bursting together are one crackle; kept by value, as the
  // payload can reuse the burst firework's slot)
  std::optional<cyclone::Vector3> highestBurst;
  for (Firework *firework = fireworks; firework < fireworks + maxFireworks; firework++) {
    // Check if we need to process this firework.
    if (firework->type > 0) {
      // Does it need removing?
      if (firework->update(duration)) {
        // Find the appropriate rule
        FireworkRule *rule = rules + (firework->type - 1);
        if (rule->payloadCount > 0 && firework->getPosition().y > 0 &&
            (!highestBurst || firework->getPosition().y > highestBurst->y))
          highestBurst = firework->getPosition();

        // Delete the current firework (this doesn't affect its
        // position and velocity for passing to the create function,
        // just whether or not it is processed for rendering or
        // physics.
        firework->type = 0;

        // Add the payload
        for (unsigned i = 0; i < rule->payloadCount; i++) {
          FireworkRule::Payload *payload = rule->payloads + i;
          create(payload->type, payload->count, firework);
        }
      }
    }
  }
  // The camera looks along -z from z = 10, so the right of the screen is +x
  if (highestBurst) {
    const float across = static_cast<float>(highestBurst->x) / 8.0f;
    sounds.play("crackle", {.volume = 0.8f, .pan = panAcross(across)});
    sounds.play("blast", {.volume = 0.25f, .pan = panAcross(across), .pitch = 1.5f});
  }

  Application::update();
}

void FireworksDemo::display() {
  const static cyclone::real size = 0.1f;

  // Clear the viewport and set the camera direction
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(0.0, 4.0, 10.0, 0.0, 4.0, 0.0, 0.0, 1.0, 0.0);

  // Render each firework in turn
  glBegin(GL_QUADS);
  for (Firework *firework = fireworks; firework < fireworks + maxFireworks; firework++) {
    // Check if we need to process this firework.
    if (firework->type > 0) {
      switch (firework->type) {
      case 1:
        glColor3f(1, 0, 0);
        break;
      case 2:
        glColor3f(1, 0.5f, 0);
        break;
      case 3:
        glColor3f(1, 1, 0);
        break;
      case 4:
        glColor3f(0, 1, 0);
        break;
      case 5:
        glColor3f(0, 1, 1);
        break;
      case 6:
        glColor3f(0.4f, 0.4f, 1);
        break;
      case 7:
        glColor3f(1, 0, 1);
        break;
      case 8:
        glColor3f(1, 1, 1);
        break;
      case 9:
        glColor3f(1, 0.5f, 0.5f);
        break;
      };

      const cyclone::Vector3 &pos = firework->getPosition();
      glVertex3f(pos.x - size, pos.y - size, pos.z);
      glVertex3f(pos.x + size, pos.y - size, pos.z);
      glVertex3f(pos.x + size, pos.y + size, pos.z);
      glVertex3f(pos.x - size, pos.y + size, pos.z);

      // Render the firework's reflection
      glVertex3f(pos.x - size, -pos.y - size, pos.z);
      glVertex3f(pos.x + size, -pos.y - size, pos.z);
      glVertex3f(pos.x + size, -pos.y + size, pos.z);
      glVertex3f(pos.x - size, -pos.y + size, pos.z);
    }
  }
  glEnd();
}

void FireworksDemo::key(unsigned char key) {
  // SAKBox: a launch whooshes up
  if (key >= '1' && key <= '9')
    sounds.play("whoosh", {.volume = 0.7f, .pitch = 1.3f});
  switch (key) {
  case '1':
    create(1, 1, nullptr);
    break;
  case '2':
    create(2, 1, nullptr);
    break;
  case '3':
    create(3, 1, nullptr);
    break;
  case '4':
    create(4, 1, nullptr);
    break;
  case '5':
    create(5, 1, nullptr);
    break;
  case '6':
    create(6, 1, nullptr);
    break;
  case '7':
    create(7, 1, nullptr);
    break;
  case '8':
    create(8, 1, nullptr);
    break;
  case '9':
    create(9, 1, nullptr);
    break;
  }
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application *getApplication() { return new FireworksDemo(); }
