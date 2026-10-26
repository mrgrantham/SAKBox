/*
 * Explosion: boxes and balls colliding.
 *
 * Five boxes and five balls, all rigid bodies, start stacked and tumble
 * into a heap. generateContacts() checks every pair (box-box, box-ball,
 * ball-ball) and each against the ground, and the shared resolver handles
 * the contacts. Resting bodies fall asleep (drawn blue; a box overlapping
 * another box is green) and W wakes them. X sets off a cyclone::Explosion
 * at the centre of the ground (SAKBox: upstream never finished it): an
 * implosion, a shock wave (drawn as a red ring while it travels) and a
 * convection chimney. Edit and up modes move the first box with the mouse,
 * for setting up collisions.
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

#include <stdio.h>

#define OBJECTS 5

// Holds a transform matrix for rendering objects
// reflected in the floor.
GLfloat floorMirror[16] = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

class Ball : public cyclone::CollisionSphere {
public:
  Ball() { body = new cyclone::RigidBody; }

  ~Ball() { delete body; }

  /** Draws the box, excluding its shadow. */
  void render() {
    // Get the OpenGL transformation
    GLfloat mat[16];
    body->getGLTransform(mat);

    if (body->getAwake())
      glColor3f(1.0f, 0.7f, 0.7f);
    else
      glColor3f(0.7f, 0.7f, 1.0f);

    glPushMatrix();
    glMultMatrixf(mat);
    solidSphere(radius, 20, 20);
    glPopMatrix();
  }

  /** Draws the ground plane shadow for the box. */
  void renderShadow() {
    // Get the OpenGL transformation
    GLfloat mat[16];
    body->getGLTransform(mat);

    glPushMatrix();
    glScalef(1.0f, 0, 1.0f);
    glMultMatrixf(mat);
    solidSphere(radius, 20, 20);
    glPopMatrix();
  }

  /** Sets the box to a specific location. */
  void setState(cyclone::Vector3 position, cyclone::Quaternion orientation, cyclone::real radius,
                cyclone::Vector3 velocity) {
    body->setPosition(position);
    body->setOrientation(orientation);
    body->setVelocity(velocity);
    body->setRotation(cyclone::Vector3(0, 0, 0));
    Ball::radius = radius;

    cyclone::real mass = 4.0f * 0.3333f * 3.1415f * radius * radius * radius;
    body->setMass(mass);

    cyclone::Matrix3 tensor;
    cyclone::real coeff = 0.4f * mass * radius * radius;
    tensor.setInertiaTensorCoeffs(coeff, coeff, coeff);
    body->setInertiaTensor(tensor);

    body->setLinearDamping(0.95f);
    body->setAngularDamping(0.8f);
    body->clearAccumulators();
    body->setAcceleration(0, -10.0f, 0);

    // body->setCanSleep(false);
    body->setAwake();

    body->calculateDerivedData();
  }

  /** Positions the box at a random location. */
  void random(cyclone::Random *random) {
    const static cyclone::Vector3 minPos(-5, 5, -5);
    const static cyclone::Vector3 maxPos(5, 10, 5);
    cyclone::Random r;
    setState(random->randomVector(minPos, maxPos), random->randomQuaternion(),
             random->randomReal(0.5f, 1.5f), cyclone::Vector3());
  }
};

class Box : public cyclone::CollisionBox {
public:
  bool isOverlapping;

  Box() { body = new cyclone::RigidBody; }

  ~Box() { delete body; }

  /** Draws the box, excluding its shadow. */
  void render() {
    // Get the OpenGL transformation
    GLfloat mat[16];
    body->getGLTransform(mat);

    if (isOverlapping)
      glColor3f(0.7f, 1.0f, 0.7f);
    else if (body->getAwake())
      glColor3f(1.0f, 0.7f, 0.7f);
    else
      glColor3f(0.7f, 0.7f, 1.0f);

    glPushMatrix();
    glMultMatrixf(mat);
    glScalef(halfSize.x * 2, halfSize.y * 2, halfSize.z * 2);
    solidCube(1.0f);
    glPopMatrix();
  }

  /** Draws the ground plane shadow for the box. */
  void renderShadow() {
    // Get the OpenGL transformation
    GLfloat mat[16];
    body->getGLTransform(mat);

    glPushMatrix();
    glScalef(1.0f, 0, 1.0f);
    glMultMatrixf(mat);
    glScalef(halfSize.x * 2, halfSize.y * 2, halfSize.z * 2);
    solidCube(1.0f);
    glPopMatrix();
  }

  /** Sets the box to a specific location. */
  void setState(const cyclone::Vector3 &position, const cyclone::Quaternion &orientation,
                const cyclone::Vector3 &extents, const cyclone::Vector3 &velocity) {
    body->setPosition(position);
    body->setOrientation(orientation);
    body->setVelocity(velocity);
    body->setRotation(cyclone::Vector3(0, 0, 0));
    halfSize = extents;

    cyclone::real mass = halfSize.x * halfSize.y * halfSize.z * 8.0f;
    body->setMass(mass);

    cyclone::Matrix3 tensor;
    tensor.setBlockInertiaTensor(halfSize, mass);
    body->setInertiaTensor(tensor);

    body->setLinearDamping(0.95f);
    body->setAngularDamping(0.8f);
    body->clearAccumulators();
    body->setAcceleration(0, -10.0f, 0);

    body->setAwake();

    body->calculateDerivedData();
  }

  /** Positions the box at a random location. */
  void random(cyclone::Random *random) {
    const static cyclone::Vector3 minPos(-5, 5, -5);
    const static cyclone::Vector3 maxPos(5, 10, 5);
    const static cyclone::Vector3 minSize(0.5f, 0.5f, 0.5f);
    const static cyclone::Vector3 maxSize(4.5f, 1.5f, 1.5f);

    setState(random->randomVector(minPos, maxPos), random->randomQuaternion(),
             random->randomVector(minSize, maxSize), cyclone::Vector3());
  }
};

/**
 * The main demo class definition.
 */
class ExplosionDemo : public RigidBodyApplication {
  bool editMode, upMode;

  /**
   * Holds the number of boxes in the simulation.
   */
  const static unsigned boxes = OBJECTS;

  /** Holds the box data. */
  Box boxData[boxes];

  /** The contacts between boxes, kept from frame to frame */
  cyclone::BoxContactCache boxContacts;

  /**
   * Holds the number of balls in the simulation.
   */
  const static unsigned balls = OBJECTS;

  /** Holds the ball data. */
  Ball ballData[balls];

  /** The explosion X sets off, and whether it is still going */
  cyclone::Explosion explosion;
  bool exploding = false;

  /** Sets off the explosion at the centre of the ground. */
  void detonate();

  /** Resets the position of all the boxes and primes the explosion. */
  void reset() override;

  /** Processes the contact generation code. */
  void generateContacts() override;

  /** Processes the objects in the simulation forward in time. */
  void updateObjects(cyclone::real duration) override;

public:
  /** Creates a new demo object. */
  ExplosionDemo();

  /** Sets up the rendering. */
  void initGraphics() override;

  /** Returns the window title for the demo. */
  const char *getTitle() override;

  /** SAKBox: the overlay explaining the demo (H hides it). */
  const char *getDescription() override {
    return "Rigid boxes and balls tumbling into a heap: collision detection "
           "between\n"
           "every pair and the ground, contact resolution, and bodies sleeping "
           "once\n"
           "they settle. X: an explosion pulls them in, a shock wave (red) throws\n"
           "them out spinning, and a chimney of hot air lifts what's above it.\n"
           "E: drag moves the first box along the ground   T: drag moves it "
           "up/down\n"
           "W: wake all bodies\n" RIGID_BODY_KEYS;
  }

  /** Display the particle positions. */
  void display() override;

  /** Handles a key press. */
  void key(unsigned char key) override;

  /** Handle a mouse drag */
  void mouseDrag(int x, int y) override;
};

// Method definitions

const char *ExplosionDemo::getTitle() { return "Cyclone > Explosion Demo"; }

ExplosionDemo::ExplosionDemo() : RigidBodyApplication(), editMode(false), upMode(false) {
  // The bodies weigh up to about 30 kg, heavier than the defaults suit
  explosion.implosionForce = 300;
  explosion.peakConcussionForce = 2500;
  explosion.shockwaveSpeed = 10;
  explosion.concussionOffset = 0.5;
  explosion.peakConvectionForce = 200;
  explosion.chimneyRadius = 3;

  // Reset the position of the boxes
  reset();
}

void ExplosionDemo::detonate() {
  explosion.detonation = cyclone::Vector3(0, 0, 0);
  explosion.restart();
  exploding = true;
  sounds.blast(explosion.detonation); // SAKBox
}

void ExplosionDemo::reset() {
  Box *box = boxData;

  box++->setState(cyclone::Vector3(0, 3, 0), cyclone::Quaternion(), cyclone::Vector3(4, 1, 1),
                  cyclone::Vector3(0, 1, 0));

  if (boxes > 1) {
    box++->setState(cyclone::Vector3(0, 4.75, 2), cyclone::Quaternion(1.0, 0.1, 0.05, 0.01),
                    cyclone::Vector3(1, 1, 4), cyclone::Vector3(0, 1, 0));
  }

  // Create the random objects
  cyclone::Random random;
  for (; box < boxData + boxes; box++) {
    box->random(&random);
  }

  for (Ball *ball = ballData; ball < ballData + balls; ball++) {
    ball->random(&random);
  }

  // Reset the contacts
  cData.contactCount = 0;
  boxContacts.clear();
  exploding = false;
}

void ExplosionDemo::generateContacts() {
  // Note that this method makes a lot of use of early returns to avoid
  // processing lots of potential contacts that it hasn't got room to
  // store.

  // Create the ground plane data
  cyclone::CollisionPlane plane;
  plane.direction = cyclone::Vector3(0, 1, 0);
  plane.offset = 0;

  // Set up the collision data structure
  cData.reset(maxContacts);
  cData.friction = (cyclone::real)0.9;
  cData.restitution = (cyclone::real)0.6;
  cData.tolerance = (cyclone::real)0.1;

  // Perform exhaustive collision detection
  cyclone::Matrix4 transform, otherTransform;
  cyclone::Vector3 position, otherPosition;
  for (Box *box = boxData; box < boxData + boxes; box++) {
    // Check for collisions with the ground plane
    if (!cData.hasMoreContacts())
      return;
    cyclone::CollisionDetector::boxAndHalfSpace(*box, plane, &cData);

    // Check for collisions with each other box
    for (Box *other = box + 1; other < boxData + boxes; other++) {
      if (!cData.hasMoreContacts())
        return;
      boxContacts.boxAndBox(*box, *other, &cData);

      if (cyclone::IntersectionTests::boxAndBox(*box, *other)) {
        box->isOverlapping = other->isOverlapping = true;
      }
    }

    // Check for collisions with each ball
    for (Ball *other = ballData; other < ballData + balls; other++) {
      if (!cData.hasMoreContacts())
        return;
      cyclone::CollisionDetector::boxAndSphere(*box, *other, &cData);
    }
  }

  for (Ball *ball = ballData; ball < ballData + balls; ball++) {
    // Check for collisions with the ground plane
    if (!cData.hasMoreContacts())
      return;
    cyclone::CollisionDetector::sphereAndHalfSpace(*ball, plane, &cData);

    for (Ball *other = ball + 1; other < ballData + balls; other++) {
      // Check for collisions with the ground plane
      if (!cData.hasMoreContacts())
        return;
      cyclone::CollisionDetector::sphereAndSphere(*ball, *other, &cData);
    }
  }
}

void ExplosionDemo::updateObjects(cyclone::real duration) {
  // The explosion's forces (they wake the bodies they reach)
  if (exploding) {
    for (Box *box = boxData; box < boxData + boxes; box++)
      explosion.updateForce(box->body, duration);
    for (Ball *ball = ballData; ball < ballData + balls; ball++)
      explosion.updateForce(ball->body, duration);
    explosion.advanceTime(duration);
    exploding = explosion.isActive();
  }

  // Update the physics of each box in turn
  for (Box *box = boxData; box < boxData + boxes; box++) {
    // Run the physics
    box->body->integrate(duration);
    box->calculateInternals();
    box->isOverlapping = false;
  }

  // Update the physics of each ball in turn
  for (Ball *ball = ballData; ball < ballData + balls; ball++) {
    // Run the physics
    ball->body->integrate(duration);
    ball->calculateInternals();
  }
}

void ExplosionDemo::initGraphics() {
  GLfloat lightAmbient[] = {0.8f, 0.8f, 0.8f, 1.0f};
  GLfloat lightDiffuse[] = {0.9f, 0.95f, 1.0f, 1.0f};

  glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

  glEnable(GL_LIGHT0);

  Application::initGraphics();
}

void ExplosionDemo::display() {
  const static GLfloat lightPosition[] = {1, -1, 0, 0};
  const static GLfloat lightPositionMirror[] = {1, 1, 0, 0};

  // Update the transform matrices of each box in turn
  for (Box *box = boxData; box < boxData + boxes; box++) {
    box->calculateInternals();
    box->isOverlapping = false;
  }

  // Update the transform matrices of each ball in turn
  for (Ball *ball = ballData; ball < ballData + balls; ball++) {
    // Run the physics
    ball->calculateInternals();
  }

  // Clear the viewport and set the camera direction
  RigidBodyApplication::display();

  // Render each element in turn as a shadow
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
  glPushMatrix();
  glMultMatrixf(floorMirror);
  glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  for (Box *box = boxData; box < boxData + boxes; box++) {
    box->render();
  }
  for (Ball *ball = ballData; ball < ballData + balls; ball++) {
    ball->render();
  }
  glPopMatrix();
  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);

  // Draw some scale circles
  glColor3f(0.75, 0.75, 0.75);
  for (unsigned i = 1; i < 20; i++) {
    glBegin(GL_LINE_LOOP);
    for (unsigned j = 0; j < 32; j++) {
      float theta = 3.1415926f * j / 16.0f;
      glVertex3f(i * cosf(theta), 0.0f, i * sinf(theta));
    }
    glEnd();
  }
  glBegin(GL_LINES);
  glVertex3f(-20, 0, 0);
  glVertex3f(20, 0, 0);
  glVertex3f(0, 0, -20);
  glVertex3f(0, 0, 20);
  glEnd();

  // The shock wave, while it travels
  if (exploding && explosion.getTimePassed() < explosion.concussionDuration) {
    const float back = (float)(explosion.shockwaveSpeed * explosion.getTimePassed());
    glColor3f(0.9f, 0.2f, 0.1f);
    for (float radius : {back, back + (float)explosion.shockwaveThickness}) {
      glBegin(GL_LINE_LOOP);
      for (unsigned j = 0; j < 64; j++) {
        float theta = 3.1415926f * j / 32.0f;
        glVertex3f(radius * cosf(theta), 0.01f, radius * sinf(theta));
      }
      glEnd();
    }
  }

  // Render each shadow in turn
  glEnable(GL_BLEND);
  glColor4f(0, 0, 0, 0.1f);
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (Box *box = boxData; box < boxData + boxes; box++) {
    box->renderShadow();
  }
  for (Ball *ball = ballData; ball < ballData + balls; ball++) {
    ball->renderShadow();
  }
  glDisable(GL_BLEND);

  // Render the boxes themselves
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, lightPositionMirror);
  glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  for (Box *box = boxData; box < boxData + boxes; box++) {
    box->render();
  }
  for (Ball *ball = ballData; ball < ballData + balls; ball++) {
    ball->render();
  }
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);

  // Finish the frame, rendering any additional information
  drawDebug();
}

void ExplosionDemo::mouseDrag(int x, int y) {
  if (editMode) {
    boxData[0].body->setPosition(boxData[0].body->getPosition() +
                                 cyclone::Vector3((x - last_x) * 0.125f, 0, (y - last_y) * 0.125f));
    boxData[0].body->calculateDerivedData();
  } else if (upMode) {
    boxData[0].body->setPosition(boxData[0].body->getPosition() +
                                 cyclone::Vector3(0, (y - last_y) * 0.125f, 0));
    boxData[0].body->calculateDerivedData();
  } else {
    RigidBodyApplication::mouseDrag(x, y);
  }

  // Remember the position
  last_x = x;
  last_y = y;
}

void ExplosionDemo::key(unsigned char key) {
  switch (key) {
  case 'e':
  case 'E':
    editMode = !editMode;
    upMode = false;
    return;

  case 't':
  case 'T':
    upMode = !upMode;
    editMode = false;
    return;

  case 'x':
  case 'X':
    detonate();
    return;

  case 'w':
  case 'W':
    for (Box *box = boxData; box < boxData + boxes; box++)
      box->body->setAwake();
    for (Ball *ball = ballData; ball < ballData + balls; ball++)
      ball->body->setAwake();
    return;
  }

  RigidBodyApplication::key(key);
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application *getApplication() { return new ExplosionDemo(); }