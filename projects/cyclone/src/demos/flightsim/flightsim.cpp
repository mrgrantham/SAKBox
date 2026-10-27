/*
 * Flight sim: an aircraft flown by aerodynamic surfaces.
 *
 * The aircraft is one rigid body. A constant propeller force pushes it
 * forward, and four aerodynamic surfaces turn the air flowing past them
 * into forces at their positions on the airframe: two wings and a rudder
 * (AeroControl: their response changes with the control setting, like
 * ailerons and a rudder) and a fixed tail plane (Aero). The resulting
 * torques pitch, roll and yaw the plane. update() adds the forces,
 * integrates, and treats the ground very simply (crash if too fast).
 * SAKBox: its engine drones and the wind rushes past, both higher and the
 * wind louder the faster it flies, and a crash booms.
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
#include <cassert>
#include <stdio.h>

/**
 * The main demo class definition.
 */
class FlightSimDemo : public Application {
  cyclone::AeroControl left_wing;
  cyclone::AeroControl right_wing;
  cyclone::AeroControl rudder;
  cyclone::Aero tail;
  cyclone::RigidBody aircraft;
  cyclone::ForceRegistry registry;

  cyclone::Vector3 windspeed;

  float left_wing_control;
  float right_wing_control;
  float rudder_control;

  void resetPlane();

public:
  /** Creates a new demo object. */
  FlightSimDemo();
  ~FlightSimDemo() override;

  /** Returns the window title for the demo. */
  const char *getTitle() override;

  /** SAKBox: the overlay explaining the demo (H hides it). */
  const char *getDescription() override {
    return "An aircraft as one rigid body: a propeller pushes it and four "
           "aerodynamic\n"
           "surfaces (two wings, rudder, tail) turn the oncoming air into "
           "forces.\n"
           "Moving the control surfaces turns those forces into pitch, roll "
           "and yaw.\n"
           "W/S: pitch   A/D: roll   Q/E: yaw   X: centre controls   R: reset"
#if defined(CYCLONE_FLEX_REAL)
           "\n[ / ]: fewer / more bits of precision   N: rounding mode   F: "
           "full"
#endif
        ;
  }

  /** Display the particles. */
  void display() override;

  /** Update the particle positions. */
  void update() override;

  /** SAKBox: one fixed step of `duration` (update runs those due) */
  void step(cyclone::real_duration duration);

  /** Handle a key press. */
  void key(unsigned char key) override;
};

// Method definitions
FlightSimDemo::FlightSimDemo()
    : Application(),

      left_wing(cyclone::Matrix3(0, 0, 0, -1, -0.5f, 0, 0, 0, 0),
                cyclone::Matrix3(0, 0, 0, -0.995f, -0.5f, 0, 0, 0, 0),
                cyclone::Matrix3(0, 0, 0, -1.005f, -0.5f, 0, 0, 0, 0),
                cyclone::Vector3(-1.0f, 0.0f, -2.0f), &windspeed),

      right_wing(cyclone::Matrix3(0, 0, 0, -1, -0.5f, 0, 0, 0, 0),
                 cyclone::Matrix3(0, 0, 0, -0.995f, -0.5f, 0, 0, 0, 0),
                 cyclone::Matrix3(0, 0, 0, -1.005f, -0.5f, 0, 0, 0, 0),
                 cyclone::Vector3(-1.0f, 0.0f, 2.0f), &windspeed),

      rudder(cyclone::Matrix3(0, 0, 0, 0, 0, 0, 0, 0, 0),
             cyclone::Matrix3(0, 0, 0, 0, 0, 0, 0.01f, 0, 0),
             cyclone::Matrix3(0, 0, 0, 0, 0, 0, -0.01f, 0, 0), cyclone::Vector3(2.0f, 0.5f, 0),
             &windspeed),

      tail(cyclone::Matrix3(0, 0, 0, -1, -0.5f, 0, 0, 0, -0.1f), cyclone::Vector3(2.0f, 0, 0),
           &windspeed),

      windspeed(0, 0, 0),

      left_wing_control(0), right_wing_control(0), rudder_control(0) {
  // Set up the aircraft rigid body.
  resetPlane();

  aircraft.setMass(2.5f);
  cyclone::Matrix3 it;
  it.setBlockInertiaTensor(cyclone::Vector3(2, 1, 1), 1);
  aircraft.setInertiaTensor(it);

  aircraft.setDamping(0.8f, 0.8f);

  aircraft.setAcceleration(cyclone::Vector3::GRAVITY);
  aircraft.calculateDerivedData();

  aircraft.setAwake();
  aircraft.setCanSleep(false);

#if defined(CYCLONE_FLEX_REAL)
  // Stochastic rounding is unbiased, so lowering precision adds noise to
  // the flight instead of steadily slowing the plane. N cycles through the
  // biased modes for comparison.
  Flex::Math::activePrecision().rounding = Flex::Math::Rounding::Stochastic;
#endif

  registry.add(&aircraft, &left_wing);
  registry.add(&aircraft, &right_wing);
  registry.add(&aircraft, &rudder);
  registry.add(&aircraft, &tail);
}

FlightSimDemo::~FlightSimDemo() {}

void FlightSimDemo::resetPlane() {
  aircraft.setPosition(0, 0, 0);
  aircraft.setOrientation(1, 0, 0, 0);

  aircraft.setVelocity(0, 0, 0);
  aircraft.setRotation(0, 0, 0);
  aircraft.rememberPose(); // SAKBox: drawn here, not sliding from the crash
}

static void drawAircraft() {
  // Fuselage
  glPushMatrix();
  glTranslatef(-0.5f, 0, 0);
  glScalef(2.0f, 0.8f, 1.0f);
  solidCube(1.0f);
  glPopMatrix();

  // Rear Fuselage
  glPushMatrix();
  glTranslatef(1.0f, 0.15f, 0);
  glScalef(2.75f, 0.5f, 0.5f);
  solidCube(1.0f);
  glPopMatrix();

  // Wings
  glPushMatrix();
  glTranslatef(0, 0.3f, 0);
  glScalef(0.8f, 0.1f, 6.0f);
  solidCube(1.0f);
  glPopMatrix();

  // Rudder
  glPushMatrix();
  glTranslatef(2.0f, 0.775f, 0);
  glScalef(0.75f, 1.15f, 0.1f);
  solidCube(1.0f);
  glPopMatrix();

  // Tail-plane
  glPushMatrix();
  glTranslatef(1.9f, 0, 0);
  glScalef(0.85f, 0.1f, 2.0f);
  solidCube(1.0f);
  glPopMatrix();
}

void FlightSimDemo::display() {
  // Clear the view port and set the camera direction
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  // SAKBox: the aircraft, and the camera following it, blended between
  // steps (see Application::drawFraction)
  const cyclone::Matrix4 transform = aircraft.getBlendedTransform(drawFraction());
  cyclone::Vector3 pos = transform.getAxisVector(3);
  cyclone::Vector3 offset(4.0f + aircraft.getVelocity().magnitude(), 0, 0);
  offset = transform.transformDirection(offset);
  gluLookAt(pos.x + offset.x, pos.y + 5.0f, pos.z + offset.z, pos.x, pos.y, pos.z, 0.0, 1.0, 0.0);

  glColor3f(0.6f, 0.6f, 0.6f);
  int bx = int(pos.x);
  int bz = int(pos.z);
  glBegin(GL_QUADS);
  for (int x = -20; x <= 20; x++)
    for (int z = -20; z <= 20; z++) {
      glVertex3f(bx + x - 0.1f, 0, bz + z - 0.1f);
      glVertex3f(bx + x - 0.1f, 0, bz + z + 0.1f);
      glVertex3f(bx + x + 0.1f, 0, bz + z + 0.1f);
      glVertex3f(bx + x + 0.1f, 0, bz + z - 0.1f);
    }
  glEnd();

  // Set the transform matrix for the aircraft
  GLfloat gl_transform[16];
  transform.fillGLArray(gl_transform);
  glPushMatrix();
  glMultMatrixf(gl_transform);

  // Draw the aircraft
  glColor3f(0, 0, 0);
  drawAircraft();
  glPopMatrix();

  glColor3f(0.8f, 0.8f, 0.8f);
  glPushMatrix();
  glTranslatef(0, -1.0f - pos.y, 0);
  glScalef(1.0f, 0.001f, 1.0f);
  glMultMatrixf(gl_transform);
  drawAircraft();
  glPopMatrix();

  char buffer[256];
  snprintf(buffer, sizeof(buffer), "Altitude: %.1f | Speed %.1f", (double)aircraft.getPosition().y,
           (double)aircraft.getVelocity().magnitude());
  glColor3f(0, 0, 0);
  renderText(10.0f, 24.0f, buffer);

  snprintf(buffer, sizeof(buffer), "Left Wing: %.1f | Right Wing: %.1f | Rudder %.1f",
           (double)left_wing_control, (double)right_wing_control, (double)rudder_control);
  renderText(10.0f, 10.0f, buffer);

#if defined(CYCLONE_FLEX_REAL)
  const Flex::Math::Precision &precision = Flex::Math::activePrecision();
  if (precision.fracBits >= 52)
    snprintf(buffer, sizeof(buffer), "Precision: full | [ ] change");
  else
    snprintf(buffer, sizeof(buffer),
             "Precision: %d bits (step %g, %s) | [ ] change, N rounding, F full",
             precision.fracBits, precision.step(),
             precision.rounding == Flex::Math::Rounding::Nearest      ? "nearest"
             : precision.rounding == Flex::Math::Rounding::Stochastic ? "stochastic"
                                                                      : "truncate");
  renderText(10.0f, 38.0f, buffer);
#endif
}

void FlightSimDemo::update() {
  // SAKBox: in fixed steps (see FixedSteps), where it ran a step as long as
  // each frame
  runDueSteps([this](cyclone::real_duration seconds) { step(seconds); });

  // SAKBox: the engine and the wind, by how fast it flies (about 10 to
  // 30 m/s)
  const float speed = static_cast<float>(aircraft.getVelocity().magnitude());
  const float fast = std::clamp(speed / 30.0f, 0.0f, 1.0f);
  sounds.setLoop("engine", {.volume = 0.3f, .pitch = 0.8f + 0.6f * fast});
  sounds.setLoop("wind", {.volume = 0.1f + 0.5f * fast * fast, .pitch = 0.7f + 0.6f * fast});

  Application::update();
}

void FlightSimDemo::step(cyclone::real_duration duration) {
  // SAKBox: drawn between where it was and where this step leaves it
  aircraft.rememberPose();

  // Start with no forces or acceleration.
  aircraft.clearAccumulators();

  // Add the propeller force
  cyclone::Vector3 propulsion(-10.0f, 0, 0);
  propulsion = aircraft.getTransform().transformDirection(propulsion);
  aircraft.addForce(propulsion);

  // Add the forces acting on the aircraft.
  registry.updateForces(duration);

  // Update the aircraft's physics.
  aircraft.integrate(duration);

  // Do a very basic collision detection and response with the ground.
  cyclone::Vector3 pos = aircraft.getPosition();
  if (pos.y < 0.0f) {
    pos.y = 0.0f;
    aircraft.setPosition(pos);

    if (aircraft.getVelocity().y < -10.0f) {
      sounds.play("blast", {.volume = 0.8f}); // SAKBox
      resetPlane();
    }
  }
}

const char *FlightSimDemo::getTitle() { return "Cyclone > Flight Sim Demo"; }

void FlightSimDemo::key(unsigned char key) {
  switch (key) {
  case 'q':
  case 'Q':
    rudder_control += 0.1f;
    break;

  case 'e':
  case 'E':
    rudder_control -= 0.1f;
    break;

  case 'w':
  case 'W':
    left_wing_control -= 0.1f;
    right_wing_control -= 0.1f;
    break;

  case 's':
  case 'S':
    left_wing_control += 0.1f;
    right_wing_control += 0.1f;
    break;

  case 'd':
  case 'D':
    left_wing_control -= 0.1f;
    right_wing_control += 0.1f;
    break;

  case 'a':
  case 'A':
    left_wing_control += 0.1f;
    right_wing_control -= 0.1f;
    break;

  case 'x':
  case 'X':
    left_wing_control = 0.0f;
    right_wing_control = 0.0f;
    rudder_control = 0.0f;
    break;

  case 'r':
  case 'R':
    resetPlane();
    break;

#if defined(CYCLONE_FLEX_REAL)
  // Whole-engine precision, down to 2 bits (steps of 1/4). The time step
  // and drag factors keep full precision (see RigidBody::integrate).
  case '[': {
    int &bits = Flex::Math::activePrecision().fracBits;
    bits = bits >= 52 ? 24 : (bits > 2 ? bits - 1 : 2);
    break;
  }

  case ']': {
    int &bits = Flex::Math::activePrecision().fracBits;
    bits = bits >= 24 ? 52 : bits + 1;
    break;
  }

  case 'n':
  case 'N': {
    // stochastic -> truncate -> nearest -> stochastic
    Flex::Math::Rounding &rounding = Flex::Math::activePrecision().rounding;
    rounding = rounding == Flex::Math::Rounding::Stochastic   ? Flex::Math::Rounding::TowardZero
               : rounding == Flex::Math::Rounding::TowardZero ? Flex::Math::Rounding::Nearest
                                                              : Flex::Math::Rounding::Stochastic;
  } break;

  case 'f':
  case 'F':
    Flex::Math::activePrecision().fracBits = 52;
    break;
#endif

  default:
    Application::key(key);
  }

  // Make sure the controls are in range
  if (left_wing_control < -1.0f)
    left_wing_control = -1.0f;
  else if (left_wing_control > 1.0f)
    left_wing_control = 1.0f;
  if (right_wing_control < -1.0f)
    right_wing_control = -1.0f;
  else if (right_wing_control > 1.0f)
    right_wing_control = 1.0f;
  if (rudder_control < -1.0f)
    rudder_control = -1.0f;
  else if (rudder_control > 1.0f)
    rudder_control = 1.0f;

  // Update the control surfaces
  left_wing.setControl(left_wing_control);
  right_wing.setControl(right_wing_control);
  rudder.setControl(rudder_control);
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application *getApplication() { return new FlightSimDemo(); }