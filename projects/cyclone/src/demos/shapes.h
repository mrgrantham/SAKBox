// SAKBox: drawing helpers that replace GLUT's (the demos now use GLFW,
// which has no shapes or fonts). Same geometry as GLUT's versions.
#ifndef CYCLONE_DEMOS_SHAPES_H
#define CYCLONE_DEMOS_SHAPES_H

// A sphere of `radius` at the origin, with smooth normals, built from
// `slices` around and `stacks` from pole to pole (glutSolidSphere).
void solidSphere(double radius, int slices, int stacks);

// A cube of edge `size` centred on the origin, with flat face normals
// (glutSolidCube).
void solidCube(double size);

// A capsule along the Y axis, centred on the origin: a cylinder of `radius`
// from -halfLength to +halfLength with a sphere at each end (as
// cyclone::CollisionCapsule), built from `slices` around.
void solidCapsule(double radius, double halfLength, int slices);

#endif // CYCLONE_DEMOS_SHAPES_H
