// FlexMatrix.hpp - a row-major matrix of FlexPoints.

#pragma once

#include <FlexVector.hpp>
namespace Flex::Math {

// A matrix of FlexPoints of any size, stored as rows. All arithmetic is
// deterministic.
//
// Construction and arithmetic check dimensions and throw
// std::invalid_argument when they don't fit: rows of different lengths, sums
// of differently sized matrices, or products where the left matrix's column
// count differs from the right matrix's row count.
//
// Example:
//   FlexMatrix a({{1, 2}, {3, 4}});
//   FlexMatrix b({{5, 6}, {7, 8}});
//   FlexMatrix c = a * b;                 // {{19, 22}, {43, 50}}
//   FlexMatrix half = a * FlexPoint(0.5); // {{0.5, 1}, {1.5, 2}}
class FlexMatrix {
private:
  std::vector<FlexVector> _matrix;

public:
  // From rows, e.g. FlexMatrix({{1, 2}, {3, 4}}). Every row must have the
  // same length.
  FlexMatrix(std::initializer_list<FlexVector> matrix);
  // An empty (0 x 0) matrix.
  FlexMatrix();

  size_t rows() const { return _matrix.size(); }
  size_t columns() const { return _matrix.empty() ? 0 : _matrix.front().size(); }

  // Unchecked access to a row.
  const FlexVector &operator[](size_t row) const { return _matrix[row]; }

  FlexMatrix operator+(const FlexMatrix &b) const; // element-wise
  FlexMatrix operator-(const FlexMatrix &b) const; // element-wise
  FlexMatrix
  operator*(const FlexMatrix &b) const;           // matrix product, O(rows * columns * b.columns())
  FlexMatrix operator*(const FlexPoint &b) const; // every element times `b`

  FlexMatrix &operator+=(const FlexMatrix &b);
  FlexMatrix &operator-=(const FlexMatrix &b);
  FlexMatrix &operator*=(const FlexMatrix &b); // *this = *this * b

  // Every element as seen at `precision` (see FlexPoint::quantized).
  FlexMatrix quantized(const Precision &precision) const;

  // True when both have the same dimensions and identical elements. Matrices,
  // like vectors, have no natural ordering, so <, >, <= and >= are not
  // provided.
  bool operator==(const FlexMatrix &b) const;
  bool operator!=(const FlexMatrix &b) const { return !(*this == b); }
};

} // namespace Flex::Math
