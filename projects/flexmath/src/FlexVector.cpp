#include "FlexVector.hpp"

#include <stdexcept>

namespace Flex::Math {

namespace {

void requireSameSize(size_t a, size_t b) {
  if (a != b) {
    throw std::invalid_argument("FlexVector: size mismatch");
  }
}

} // namespace

FlexVector::FlexVector(std::initializer_list<FlexPoint> vector) : _vector(vector) {}

FlexVector::FlexVector() {}

FlexVector FlexVector::operator+(const FlexVector &vector) const {
  FlexVector flexVector = *this;
  return flexVector += vector;
}

FlexVector FlexVector::operator-(const FlexVector &vector) const {
  FlexVector flexVector = *this;
  return flexVector -= vector;
}

FlexVector FlexVector::operator*(const FlexVector &vector) const {
  FlexVector flexVector = *this;
  return flexVector *= vector;
}

FlexVector FlexVector::operator*(const FlexPoint &value) const {
  FlexVector flexVector = *this;
  for (auto &element : flexVector._vector) {
    element *= value;
  }
  return flexVector;
}

FlexVector &FlexVector::operator+=(const FlexVector &vector) {
  requireSameSize(_vector.size(), vector._vector.size());
  for (size_t i = 0; i < _vector.size(); i++) {
    _vector[i] += vector._vector[i];
  }
  return *this;
}

FlexVector &FlexVector::operator-=(const FlexVector &vector) {
  requireSameSize(_vector.size(), vector._vector.size());
  for (size_t i = 0; i < _vector.size(); i++) {
    _vector[i] -= vector._vector[i];
  }
  return *this;
}

FlexVector &FlexVector::operator*=(const FlexVector &vector) {
  requireSameSize(_vector.size(), vector._vector.size());
  for (size_t i = 0; i < _vector.size(); i++) {
    _vector[i] *= vector._vector[i];
  }
  return *this;
}

FlexPoint FlexVector::dot(const FlexVector &vector) const {
  requireSameSize(_vector.size(), vector._vector.size());
  FlexPoint sum;
  for (size_t i = 0; i < _vector.size(); i++) {
    sum += _vector[i] * vector._vector[i];
  }
  return sum;
}

FlexVector FlexVector::quantized(const Precision &precision) const {
  FlexVector flexVector = *this;
  for (auto &element : flexVector._vector) {
    element = element.quantized(precision);
  }
  return flexVector;
}

} // namespace Flex::Math
