// SAKBox: replacements for glutSolidSphere and glutSolidCube, and capsules.
#include "shapes.h"

#include "ogl_headers.h"

#include <initializer_list>

// A filled, smoothly shaded GLU quadric, as GLUT's solid shapes are
static GLUquadric *solidQuadric() {
  static GLUquadric *quadric = [] {
    GLUquadric *q = gluNewQuadric();
    gluQuadricDrawStyle(q, GLU_FILL);
    gluQuadricNormals(q, GLU_SMOOTH);
    return q;
  }();
  return quadric;
}

void solidSphere(double radius, int slices, int stacks) {
  gluSphere(solidQuadric(), radius, slices, stacks);
}

void solidCube(double size) {
  // Faces and normals as in GLUT's drawBox
  static const GLfloat normals[6][3] = {{-1, 0, 0}, {0, 1, 0}, {1, 0, 0},
                                        {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  static const GLint faces[6][4] = {{0, 1, 2, 3}, {3, 2, 6, 7}, {7, 6, 5, 4},
                                    {4, 5, 1, 0}, {5, 6, 2, 1}, {7, 4, 0, 3}};
  const GLfloat h = static_cast<GLfloat>(size / 2);
  GLfloat v[8][3];
  v[0][0] = v[1][0] = v[2][0] = v[3][0] = -h;
  v[4][0] = v[5][0] = v[6][0] = v[7][0] = h;
  v[0][1] = v[1][1] = v[4][1] = v[5][1] = -h;
  v[2][1] = v[3][1] = v[6][1] = v[7][1] = h;
  v[0][2] = v[3][2] = v[4][2] = v[7][2] = -h;
  v[1][2] = v[2][2] = v[5][2] = v[6][2] = h;

  glBegin(GL_QUADS);
  for (int i = 5; i >= 0; i--) {
    glNormal3fv(normals[i]);
    for (int j = 0; j < 4; j++)
      glVertex3fv(v[faces[i][j]]);
  }
  glEnd();
}

void solidCapsule(double radius, double halfLength, int slices) {
  for (double end : {-halfLength, halfLength}) {
    glPushMatrix();
    glTranslated(0, end, 0);
    solidSphere(radius, slices, slices / 2);
    glPopMatrix();
  }
  // GLU's cylinder runs up the Z axis from zero: turn it onto Y, starting at
  // the lower end
  glPushMatrix();
  glTranslated(0, -halfLength, 0);
  glRotated(-90, 1, 0, 0);
  gluCylinder(solidQuadric(), radius, radius, halfLength * 2, slices, 1);
  glPopMatrix();
}
