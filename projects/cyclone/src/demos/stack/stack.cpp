/*
 * SAKBox: Stack: a tower of boxes kept awake, to compare contact resolvers.
 *
 * Ten 1 m boxes are stacked on the ground and never allowed to sleep, so
 * the contact resolver has to hold them up every frame. Resolving the
 * contacts together (cyclone::ContactResolver::Method::sequentialImpulses,
 * the default) keeps the tower standing still; M switches to the
 * book's resolver, which handles one contact at a time, and the tower
 * creeps sideways and falls (README, known limitations). 1-9 and 0 rebuild
 * the tower with that many boxes (0 is ten). Awake boxes are red.
 *
 * Built on the explosion demo's boxes and drawing.
 */

#include "../app.h"
#include "../ogl_headers.h"
#include <cyclone/cyclone.h>

#include <memory>
#include <vector>

// Holds a transform matrix for rendering objects reflected in the floor
static GLfloat floorMirror[16] = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

namespace {

// A 1 m box of 1 kg in the tower
class Box : public cyclone::CollisionBox {
public:
  Box() {
    body = new cyclone::RigidBody;
    halfSize = cyclone::Vector3(0.5, 0.5, 0.5);
  }
  ~Box() { delete body; }
  Box(const Box &) = delete;
  Box &operator=(const Box &) = delete;

  // Places the box at `height` (its centre), still and awake, unable to
  // sleep
  void placeAt(cyclone::real height) {
    body->setPosition(cyclone::Vector3(0, height, 0));
    body->setOrientation(cyclone::Quaternion());
    body->setVelocity(cyclone::Vector3());
    body->setRotation(cyclone::Vector3());
    body->setMass(1);
    cyclone::Matrix3 tensor;
    tensor.setBlockInertiaTensor(halfSize, 1);
    body->setInertiaTensor(tensor);
    body->setLinearDamping(0.95f);
    body->setAngularDamping(0.8f);
    body->clearAccumulators();
    body->setAcceleration(cyclone::Vector3::GRAVITY);
    body->setCanSleep(false);
    body->setAwake();
    body->calculateDerivedData();
    calculateInternals();
  }

  // Draws the box (red: awake, blue: asleep)
  void render() const {
    GLfloat mat[16];
    body->getGLTransform(mat);
    if (body->getAwake())
      glColor3f(1.0f, 0.7f, 0.7f);
    else
      glColor3f(0.7f, 0.7f, 1.0f);
    glPushMatrix();
    glMultMatrixf(mat);
    solidCube(1.0f);
    glPopMatrix();
  }

  // Draws its shadow on the ground
  void renderShadow() const {
    GLfloat mat[16];
    body->getGLTransform(mat);
    glPushMatrix();
    glScalef(1.0f, 0, 1.0f);
    glMultMatrixf(mat);
    solidCube(1.0f);
    glPopMatrix();
  }
};

} // namespace

class StackDemo : public RigidBodyApplication {
  // The boxes, bottom first. Boxes are held by pointer because each owns
  // its body.
  std::vector<std::unique_ptr<Box>> boxes;
  unsigned height = 10;

  // The contacts between boxes, kept from frame to frame
  cyclone::BoxContactCache boxContacts;

  void reset() override;
  void generateContacts() override;
  void updateObjects(cyclone::real duration) override;

public:
  StackDemo();

  void initGraphics() override;
  const char *getTitle() override { return "Cyclone > Stack Demo"; }

  const char *getDescription() override {
    return "A tower of boxes that are never allowed to sleep, so the contact\n"
           "resolver holds them up every frame. Resolving the contacts together\n"
           "keeps it standing still; M switches to the book's resolver, which\n"
           "handles one contact at a time: the tower creeps and falls.\n"
           "1-9, 0: rebuild with that many boxes (0: ten)\n" RIGID_BODY_KEYS;
  }

  void display() override;
  void key(unsigned char key) override;
};

StackDemo::StackDemo() {
  // Turned, so two sides of each box show
  theta = 30;
  resolver.setMethod(cyclone::ContactResolver::Method::sequentialImpulses);
  reset();
}

void StackDemo::reset() {
  boxes.clear();
  for (unsigned i = 0; i < height; i++) {
    boxes.push_back(std::make_unique<Box>());
    boxes.back()->placeAt((cyclone::real)0.5 + i);
  }
  cData.contactCount = 0;
  boxContacts.clear();
  resolver.forgetWarmStart();
}

void StackDemo::generateContacts() {
  cyclone::CollisionPlane ground;
  ground.direction = cyclone::Vector3(0, 1, 0);
  ground.offset = 0;

  cData.reset(maxContacts);
  cData.friction = (cyclone::real)0.9;
  cData.restitution = (cyclone::real)0.1;
  cData.tolerance = (cyclone::real)0.1;

  for (size_t i = 0; i < boxes.size(); i++) {
    if (!cData.hasMoreContacts())
      return;
    cyclone::CollisionDetector::boxAndHalfSpace(*boxes[i], ground, &cData);
    for (size_t j = i + 1; j < boxes.size(); j++) {
      if (!cData.hasMoreContacts())
        return;
      boxContacts.boxAndBox(*boxes[i], *boxes[j], &cData);
    }
  }
}

void StackDemo::updateObjects(cyclone::real duration) {
  for (const auto &box : boxes) {
    box->body->integrate(duration);
    box->calculateInternals();
  }
}

void StackDemo::initGraphics() {
  // The boxes' colour sets their ambient as well as diffuse light, so every
  // side shows it: half the light is ambient
  GLfloat lightAmbient[] = {0.5f, 0.5f, 0.5f, 1.0f};
  GLfloat lightDiffuse[] = {0.5f, 0.5f, 0.55f, 1.0f};
  glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
  glEnable(GL_LIGHT0);
  Application::initGraphics();
}

void StackDemo::display() {
  const static GLfloat lightPosition[] = {1, -1, 0, 0};
  const static GLfloat lightPositionMirror[] = {1, 1, 0, 0};

  RigidBodyApplication::display();
  // Shrink the scene about the point the camera looks at (5 m up), so the
  // whole tower fits below the overlay
  glTranslatef(0, 5, 0);
  glScalef(0.75f, 0.75f, 0.75f);
  glTranslatef(0, -5, 0);
  glEnable(GL_NORMALIZE); // the scale shrinks the normals too

  // The reflections in the floor
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
  glPushMatrix();
  glMultMatrixf(floorMirror);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  for (const auto &box : boxes)
    box->render();
  glPopMatrix();
  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);

  // A grid on the ground, 1 m apart, to show any creep
  glColor3f(0.75, 0.75, 0.75);
  glBegin(GL_LINES);
  for (int i = -10; i <= 10; i++) {
    glVertex3f((float)i, 0, -10);
    glVertex3f((float)i, 0, 10);
    glVertex3f(-10, 0, (float)i);
    glVertex3f(10, 0, (float)i);
  }
  glEnd();

  // The shadows
  glEnable(GL_BLEND);
  glColor4f(0, 0, 0, 0.1f);
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (const auto &box : boxes)
    box->renderShadow();
  glDisable(GL_BLEND);

  // The boxes
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, lightPositionMirror);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  for (const auto &box : boxes)
    box->render();
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);

  drawDebug();
}

void StackDemo::key(unsigned char key) {
  if (key >= '0' && key <= '9') {
    height = key == '0' ? 10 : key - '0';
    reset();
    return;
  }
  RigidBodyApplication::key(key);
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application *getApplication() { return new StackDemo(); }
