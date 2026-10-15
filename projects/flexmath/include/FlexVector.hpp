// FlexVector.hpp - a variable-length vector of FlexPoints.

#pragma once
#include <FlexPoint.hpp>
#include <initializer_list>
#include <vector>

namespace Flex::Math {

// A vector of FlexPoints of any length. Like FlexPoint, all arithmetic is
// deterministic.
//
// Operations between two vectors are element-wise and throw
// std::invalid_argument if the sizes differ.
//
// Example:
//   FlexVector a({1, 2, 3}), b({0.5, 0.5, 0.5});
//   FlexVector sum = a + b;                       // {1.5, 2.5, 3.5}
//   FlexPoint d = a.dot(b);                       // 3
//   FlexVector seen = sum.quantized(Precision{.fracBits = 0});   // {1, 2, 3}
class FlexVector {
private:
  std::vector<FlexPoint> _vector;

public:
  FlexVector(std::initializer_list<FlexPoint> vector);
  // An empty vector.
  FlexVector();

  size_t size() const { return _vector.size(); }
  void push_back(const FlexPoint &value) { _vector.push_back(value); }

  // Unchecked element access, like std::vector.
  FlexPoint &operator[](size_t i) { return _vector[i]; }
  const FlexPoint &operator[](size_t i) const { return _vector[i]; }

  FlexVector operator+(const FlexVector &vector) const;
  FlexVector operator-(const FlexVector &vector) const;
  FlexVector operator*(const FlexVector &vector) const; // element-wise product
  FlexVector operator*(const FlexPoint &value) const;   // every element times `value`

  FlexVector &operator+=(const FlexVector &vector);
  FlexVector &operator-=(const FlexVector &vector);
  FlexVector &operator*=(const FlexVector &vector);

  // Sum of element-wise products. Throws std::invalid_argument on a size
  // mismatch.
  FlexPoint dot(const FlexVector &vector) const;

  // Every element as seen at `precision` (see FlexPoint::quantized).
  FlexVector quantized(const Precision &precision) const;

  // Vectors have no natural ordering, so <, >, <= and >= are not provided.
  auto operator<=>(const FlexVector &vector) = delete;

  // True when both have the same size and identical elements.
  bool operator==(const FlexVector &vector) const { return _vector == vector._vector; }
};

} // namespace Flex::Math
