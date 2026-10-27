/*
 * The definition file for the default application object.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */
#include "app.h"
#include "ogl_headers.h"
#include "timing.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

// stb_easy_font's helpers are static; not all of them are used
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <stb/stb_easy_font.h>
#pragma GCC diagnostic pop

void Application::initGraphics() {
  glClearColor(0.9f, 0.95f, 1.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);

  setView();
}

void Application::setView() {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, (double)width / (double)height, 1.0, 500.0);
  glMatrixMode(GL_MODELVIEW);
}

void Application::display() {
  glClear(GL_COLOR_BUFFER_BIT);

  glBegin(GL_LINES);
  glVertex2i(1, 1);
  glVertex2i(639, 319);
  glEnd();
}

const char *Application::getTitle() { return "Cyclone Demo"; }

void Application::deinit() {}

void Application::update() {
  impacts.playHits();
  sounds.update();
}

std::string Application::soundState() {
  if (!sounds.playing())
    return "No sound: " + sounds.error();
  if (!sounds.enabled())
    return "Sound: off (V)";
  return "Sound: on (V), " + std::to_string(sounds.played()) + " played";
}

void Application::key(unsigned char key) {}

void Application::resize(int width, int height) {
  // Avoid the divide by zero.
  if (height <= 0)
    height = 1;

  // Set the internal variables and update the view
  Application::width = width;
  Application::height = height;
  glViewport(0, 0, width, height);
  // SAKBox: lines as thick on screen as under GLUT, which drew high-DPI
  // displays at half resolution
  glLineWidth(pixelScale);
  setView();
}

void Application::mouse(int button, int state, int x, int y) {}

void Application::mouseDrag(int x, int y) {}

// The following methods aren't intended to be overloaded
void Application::renderOverlay() {
  // SAKBox: the sound's state, bottom right
  const std::string state = soundState();
  const float stateMargin = 2;
  const float stateWidth = (float)stb_easy_font_width(const_cast<char *>(state.c_str()));
  renderPanel(width / pixelScale - stateWidth - 10.0f - stateMargin,
              soundStateHeight + 8.0f + stateMargin, stateMargin, state);

  const char *description = getDescription();
  if (!description)
    return;
  std::string text = showOverlay ? std::string(description) + "\n\nH: hide   V: sound on/off"
                                 : std::string("H: what is this demo?");
  // 8 points in from the top-left corner
  const float inset = 8;
  renderPanel(inset, height / pixelScale - inset, 6, text);
}

void Application::renderPanel(float left, float top, float margin, const std::string &text) {
  // The panel's size in points, from stb_easy_font's measurements, with a
  // margin
  const float textWidth = (float)stb_easy_font_width(const_cast<char *>(text.c_str()));
  const float textHeight = (float)stb_easy_font_height(const_cast<char *>(text.c_str()));
  const float right = left + textWidth + 2 * margin;
  const float bottom = top - textHeight - 2 * margin;

  glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // A translucent white panel, so the text reads on light and dark scenes
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, (double)width, 0.0, (double)height, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glScalef(pixelScale, pixelScale, 1);
  glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  glBegin(GL_QUADS);
  glVertex2f(left, bottom);
  glVertex2f(right, bottom);
  glVertex2f(right, top);
  glVertex2f(left, top);
  glEnd();
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  // renderText takes the top line's baseline position
  glColor3f(0.1f, 0.1f, 0.15f);
  renderText(left + margin, top - margin - 8.0f, text.c_str());
  glPopAttrib();
}

void Application::renderText(float x, float y, const char *text) {
  glDisable(GL_DEPTH_TEST);

  // Temporarily set up the view in orthographic projection.
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, (double)width, 0.0, (double)height, -1.0, 1.0);

  // Move to modelview mode.
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // SAKBox: GLUT's bitmap font is replaced by stb_easy_font, which builds
  // the text from quads (12 points per line, as before). Its y axis points
  // down, so flip it, and scale points to pixels.
  // stb_easy_font needs about 270 bytes of vertices per character
  std::vector<char> quads(std::strlen(text) * 300 + 64);
  int count =
      stb_easy_font_print(0, 0, const_cast<char *>(text), nullptr, quads.data(), (int)quads.size());
  glTranslatef(x * pixelScale, (y + 8.0f) * pixelScale, 0);
  glScalef(pixelScale, -pixelScale, 1);
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 16, quads.data());
  glDrawArrays(GL_QUADS, 0, count * 4);
  glDisableClientState(GL_VERTEX_ARRAY);

  // Pop the matrices to return to how we were before.
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  glEnable(GL_DEPTH_TEST);
}

MassAggregateApplication::MassAggregateApplication(unsigned int particleCount)
    : world(particleCount * 10) {
  particleArray = new cyclone::Particle[particleCount];
  for (unsigned i = 0; i < particleCount; i++) {
    world.getParticles().push_back(particleArray + i);
  }

  groundContactGenerator.init(&world.getParticles());
  world.getContactGenerators().push_back(&groundContactGenerator);
  hearContacts(world); // SAKBox: knocks on the ground, and as cables snap taut
}

MassAggregateApplication::~MassAggregateApplication() { delete[] particleArray; }

void MassAggregateApplication::initGraphics() {
  // Call the superclass
  Application::initGraphics();
}

void MassAggregateApplication::display() {
  // Clear the view port and set the camera direction
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(0.0, 3.5, 8.0, 0.0, 3.5, 0.0, 0.0, 1.0, 0.0);

  glColor3f(0, 0, 0);

  cyclone::ParticleWorld::Particles &particles = world.getParticles();
  for (cyclone::ParticleWorld::Particles::iterator p = particles.begin(); p != particles.end();
       p++) {
    cyclone::Particle *particle = *p;
    const cyclone::Vector3 &pos = particle->getPosition();
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    solidSphere(0.1f, 20, 10);
    glPopMatrix();
  }
}

void MassAggregateApplication::update() {
  // Clear accumulators
  world.startFrame();

  // Find the duration of the last frame in seconds
  float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
  if (duration <= 0.0f)
    return;

  // Run the simulation
  world.runPhysics(duration);

  Application::update();
}

RigidBodyApplication::RigidBodyApplication()
    : theta(0.0f), phi(15.0f), renderDebugInfo(false), pauseSimulation(true),
      autoPauseSimulation(false) {
  soundStateHeight = 25.0f; // between the resolver and the steps
}

void RigidBodyApplication::update() {
  // Exit immediately if we aren't running the simulation
  if (pauseSimulation) {
    Application::update();
    return;
  }

  // SAKBox: the steps this frame covers (see FixedSteps), or just one
  // (Space)
  unsigned steps = 1;
  if (autoPauseSimulation) {
    pauseSimulation = true;
    autoPauseSimulation = false;
  } else {
    steps = physicsClock.stepsFor(TimingData::get().lastFrameDuration * 0.001);
  }

  impacts.setCameraAngle(theta);
  double took = 0;
  for (unsigned i = 0; i < steps; ++i) {
    runStep();
    const cyclone::World::Profile &profile = getWorld().profile();
    took += profile.findContacts + profile.integrate + profile.resolve;
  }
  physicsMilliseconds += 0.05 * (took - physicsMilliseconds);

  Application::update();
}

void RigidBodyApplication::runStep() {
  cyclone::World &world = getWorld();
  // The hits among the contacts, before they're resolved, and the demo's
  // look at them (set each step, as the world is the demo's)
  world.contactsFound = [this](std::span<const cyclone::Contact> contacts,
                               cyclone::real_duration seconds) {
    impacts.hear(contacts, static_cast<float>(seconds));
    contactsFound(contacts);
  };
  world.setSubSteps(physicsClock.subSteps());
  world.startFrame();
  beforeStep();
  world.runPhysics(physicsClock.stepSeconds());
  afterStep();
}

void RigidBodyApplication::display() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(cameraDistance, 0, 0, 0, 0, 0, 0, 1.0f, 0);
  glRotatef(-phi, 0, 0, 1);
  glRotatef(theta, 0, 1, 0);
  glTranslatef(0, -cameraTargetHeight, 0);
}

void RigidBodyApplication::renderResolverMethod() {
  const char *text =
      getWorld().getResolver().getMethod() == cyclone::ContactResolver::Method::sequentialImpulses
          ? "Resolver: all contacts together"
          : "Resolver: one contact at a time (the book's)";
  const float textWidth = (float)stb_easy_font_width(const_cast<char *>(text));
  glColor3f(0.1f, 0.1f, 0.15f);
  renderText(width / pixelScale - textWidth - 10.0f, 10.0f, text);
}

std::string RigidBodyApplication::soundState() {
  if (!sounds.playing() || !sounds.enabled())
    return Application::soundState();
  return "Sound: on (V), " + std::to_string(impacts.knocks()) + " knocks, " +
         std::to_string(impacts.dropped()) + " too quiet to fit";
}

void RigidBodyApplication::renderPhysicsSteps() {
  char text[128];
  std::snprintf(
      text, sizeof text,
      "Physics: %d steps a second, %u sub-step%s each ([ ]), drawn %s (B), %.2f ms a frame",
      static_cast<int>(std::lround(1 / physicsClock.stepSeconds())), physicsClock.subSteps(),
      physicsClock.subSteps() == 1 ? "" : "s", blendDrawing ? "blended" : "unblended",
      physicsMilliseconds);
  const float textWidth = (float)stb_easy_font_width(text);
  glColor3f(0.1f, 0.1f, 0.15f);
  renderText(width / pixelScale - textWidth - 10.0f, 40.0f, text);
}

void RigidBodyApplication::renderCornerStatus() {
  renderResolverMethod();
  renderPhysicsSteps();
}

void RigidBodyApplication::drawDebug() {
  renderCornerStatus();
  if (!renderDebugInfo)
    return;

  // SAKBox: the last step's contacts (where they were found, with those a
  // little apart that sub-steps follow)
  glBegin(GL_LINES);
  for (const cyclone::Contact &contact : getWorld().getStepContacts()) {
    // Interbody contacts are in green, floor contacts are red.
    if (contact.body[1]) {
      glColor3f(0, 1, 0);
    } else {
      glColor3f(1, 0, 0);
    }

    cyclone::Vector3 vec = contact.contactPoint;
    glVertex3f(vec.x, vec.y, vec.z);

    vec += contact.contactNormal;
    glVertex3f(vec.x, vec.y, vec.z);
  }

  glEnd();
}

void RigidBodyApplication::mouse(int button, int state, int x, int y) {
  // Set the position
  last_x = x;
  last_y = y;
}

void RigidBodyApplication::mouseDrag(int x, int y) {
  // Update the camera
  theta += (x - last_x) * 0.25f;
  phi += (y - last_y) * 0.25f;

  // Keep it in bounds
  if (phi < -20.0f)
    phi = -20.0f;
  else if (phi > 80.0f)
    phi = 80.0f;

  // Remember the position
  last_x = x;
  last_y = y;
}

void RigidBodyApplication::key(unsigned char key) {
  switch (key) {
  case 'R':
  case 'r':
    // Reset the simulation
    reset();
    // SAKBox: drawn where they are now, not sliding from before the reset
    getWorld().rememberPoses();
    return;

  case 'B':
  case 'b':
    // SAKBox: draw blended between steps, or as the last step left them
    blendDrawing = !blendDrawing;
    return;

  case 'C':
  case 'c':
    // Toggle rendering of contacts
    renderDebugInfo = !renderDebugInfo;
    return;

  case 'P':
  case 'p':
    // Toggle running the simulation
    pauseSimulation = !pauseSimulation;
    return;

  case ' ':
    // Advance one step
    autoPauseSimulation = true;
    pauseSimulation = false;
    break;

  case '[':
  case ']':
    // SAKBox: fewer or more sub-steps a step
    physicsClock.setSubSteps(physicsClock.subSteps() + (key == ']' ? 1 : -1));
    return;

  case 'M':
  case 'm':
    // SAKBox: switch between the book's resolver and resolving the contacts
    // together, to compare them
    {
      cyclone::ContactResolver &resolver = getWorld().getResolver();
      resolver.setMethod(resolver.getMethod() ==
                                 cyclone::ContactResolver::Method::sequentialImpulses
                             ? cyclone::ContactResolver::Method::oneContactAtATime
                             : cyclone::ContactResolver::Method::sequentialImpulses);
      resolver.forgetWarmStart();
      return;
    }
  }

  Application::key(key);
}
