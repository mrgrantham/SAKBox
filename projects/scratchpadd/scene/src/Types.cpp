#include <ScratchPadd/Scene/Types.hpp>

namespace ScratchPadd::Scene {

Transform Transform::translation(const Vector3 &v) {
  Transform t;
  t.m[0][3] = v.x;
  t.m[1][3] = v.y;
  t.m[2][3] = v.z;
  return t;
}

Transform Transform::scaling(const Vector3 &v) {
  Transform t;
  t.m[0][0] = v.x;
  t.m[1][1] = v.y;
  t.m[2][2] = v.z;
  return t;
}

Transform Transform::rotation(const Vector3 &axis, Scalar radians) {
  // Rodrigues' formula: cos I + sin [axis]x + (1 - cos) axis axis^T
  const Vector3 a = axis.normalised();
  const Scalar c = std::cos(radians), s = std::sin(radians), k = 1 - c;
  Transform t;
  t.m[0] = {c + a.x * a.x * k, a.x * a.y * k - a.z * s, a.x * a.z * k + a.y * s, 0};
  t.m[1] = {a.y * a.x * k + a.z * s, c + a.y * a.y * k, a.y * a.z * k - a.x * s, 0};
  t.m[2] = {a.z * a.x * k - a.y * s, a.z * a.y * k + a.x * s, c + a.z * a.z * k, 0};
  return t;
}

Transform Transform::fromAxes(const Vector3 &x, const Vector3 &y, const Vector3 &z,
                              const Vector3 &position) {
  Transform t;
  t.m[0] = {x.x, y.x, z.x, position.x};
  t.m[1] = {x.y, y.y, z.y, position.y};
  t.m[2] = {x.z, y.z, z.z, position.z};
  return t;
}

Transform Transform::operator*(const Transform &o) const {
  Transform t;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 4; ++c) {
      Scalar sum = c == 3 ? m[r][3] : 0;
      for (int k = 0; k < 3; ++k)
        sum += m[r][k] * o.m[k][c];
      t.m[r][c] = sum;
    }
  }
  return t;
}

Vector3 Transform::apply(const Vector3 &p) const { return applyToDirection(p) + position(); }

Vector3 Transform::applyToDirection(const Vector3 &d) const {
  return {m[0][0] * d.x + m[0][1] * d.y + m[0][2] * d.z,
          m[1][0] * d.x + m[1][1] * d.y + m[1][2] * d.z,
          m[2][0] * d.x + m[2][1] * d.y + m[2][2] * d.z};
}

} // namespace ScratchPadd::Scene
