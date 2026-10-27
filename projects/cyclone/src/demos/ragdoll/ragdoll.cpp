/*
 * Ragdoll: rigid bones held together by joints.
 *
 * Twelve boxes are the bones (head, chest, arms, legs) and eleven Joints
 * link them at the shoulders, elbows, hips, knees and neck. Each joint
 * lets its two attachment points drift a little apart, like a short cable;
 * the contact resolver holds the body together along with keeping the
 * bones out of the ground (SAKBox: as a contact at each stretched joint
 * with the book's resolver, as a constraint between the two points when
 * resolving contacts together). The physics lives in ragdoll_physics.h, shared with
 * ragdoll_test; reset() stands the ragdoll up and gives it a small shove,
 * and it collapses.
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

#include "ragdoll_physics.h"

#include <algorithm>
#include <cstdio>

/** Draws a bone, `fraction` of the way from its last step to now. */
static void renderBone(const ragdoll::Bone &bone, cyclone::real fraction) {
  // Get the OpenGL transformation
  GLfloat mat[16];
  bone.body->getBlendedGLTransform(fraction, mat);

  if (bone.body->getAwake())
    glColor3f(0.5f, 0.3f, 0.3f);
  else
    glColor3f(0.3f, 0.3f, 0.5f);

  glPushMatrix();
  glMultMatrixf(mat);
  glScalef(bone.halfSize.x * 2, bone.halfSize.y * 2, bone.halfSize.z * 2);
  solidCube(1.0f);
  glPopMatrix();
}

/**
 * The main demo class definition.
 */
class RagdollDemo : public RigidBodyApplication {
  cyclone::Random random;

  /** The bones and joints, in their world (ragdoll_physics.h). */
  ragdoll::Ragdoll model;

  /**
   * SAKBox: the most any joint has stretched beyond its slack since the
   * last reset (m), to compare sub-steps ([ and ]) by
   */
  double worstStretch = 0;

  /** SAKBox: measures the joints after each step. */
  void afterStep() override { worstStretch = std::max(worstStretch, model.worstStretch()); }

  /** SAKBox: the ragdoll's world. */
  cyclone::World &getWorld() override { return model.world; }

  /** Resets the position of all the bones. */
  void reset() override;

public:
  /** Creates a new demo object. */
  RagdollDemo();

  /** Sets up the rendering. */
  void initGraphics() override;

  /** Returns the window title for the demo. */
  const char *getTitle() override;

  /** SAKBox: the overlay explaining the demo (H hides it). */
  const char *getDescription() override {
    return "A ragdoll: 12 rigid boxes for bones, linked by 11 joints that let\n"
           "their ends drift a little apart, like short cables. The contact\n"
           "resolver holds the body together. It starts with a small shove and\n"
           "collapses. Bottom left: the most a joint has stretched since R.\n" RIGID_BODY_KEYS;
  }

  /** Display the particle positions. */
  void display() override;
};

// Method definitions
RagdollDemo::RagdollDemo() : RigidBodyApplication() { reset(); }

const char *RagdollDemo::getTitle() { return "Cyclone > Ragdoll Demo"; }

void RagdollDemo::reset() {
  model.reset(random);
  worstStretch = 0;
}

void RagdollDemo::initGraphics() {
  GLfloat lightAmbient[] = {0.8f, 0.8f, 0.8f, 1.0f};
  GLfloat lightDiffuse[] = {0.9f, 0.95f, 1.0f, 1.0f};

  glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

  glEnable(GL_LIGHT0);

  Application::initGraphics();
}

void RagdollDemo::display() {
  const static GLfloat lightPosition[] = {0.7f, -1, 0.4f, 0};

  RigidBodyApplication::display();

  // Render the bones
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
  glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);

  glEnable(GL_NORMALIZE);
  glColor3f(1, 0, 0);
  for (const ragdoll::Bone &bone : model.bones) {
    renderBone(bone, drawFraction());
  }
  glDisable(GL_NORMALIZE);

  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);

  glDisable(GL_DEPTH_TEST);
  glBegin(GL_LINES);
  for (const cyclone::Joint &jointRef : model.joints) {
    const cyclone::Joint *joint = &jointRef;
    cyclone::Vector3 a_pos = joint->body[0]->getPointInWorldSpace(joint->position[0]);
    cyclone::Vector3 b_pos = joint->body[1]->getPointInWorldSpace(joint->position[1]);
    cyclone::real length = (b_pos - a_pos).magnitude();

    if (length > joint->error)
      glColor3f(1, 0, 0);
    else
      glColor3f(0, 1, 0);

    glVertex3f(a_pos.x, a_pos.y, a_pos.z);
    glVertex3f(b_pos.x, b_pos.y, b_pos.z);
  }
  glEnd();
  glEnable(GL_DEPTH_TEST);

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

  RigidBodyApplication::drawDebug();

  char text[64];
  std::snprintf(text, sizeof text, "Worst joint stretch since reset: %.2f cm", worstStretch * 100);
  glColor3f(0.1f, 0.1f, 0.15f);
  renderText(10.0f, 10.0f, text);
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application *getApplication() { return new RagdollDemo(); }