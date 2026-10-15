#include "FlexMatrix.hpp"

#include <stdexcept>

namespace Flex::Math {

FlexMatrix::FlexMatrix(std::initializer_list<FlexVector> matrix) : _matrix(matrix) {
  for (const auto &row : _matrix) {
    if (row.size() != columns()) {
      throw std::invalid_argument("FlexMatrix: rows must all be the same length");
    }
  }
}

FlexMatrix::FlexMatrix() {}

FlexMatrix FlexMatrix::operator+(const FlexMatrix &b) const {
  FlexMatrix result = *this;
  return result += b;
}

FlexMatrix FlexMatrix::operator-(const FlexMatrix &b) const {
  FlexMatrix result = *this;
  return result -= b;
}

FlexMatrix FlexMatrix::operator*(const FlexMatrix &b) const {
  if (columns() != b.rows()) {
    throw std::invalid_argument("FlexMatrix: columns of the left must match rows of the right");
  }
  FlexMatrix result;
  for (size_t r = 0; r < rows(); r++) {
    FlexVector row;
    for (size_t c = 0; c < b.columns(); c++) {
      FlexPoint sum;
      for (size_t k = 0; k < columns(); k++) {
        sum += _matrix[r][k] * b._matrix[k][c];
      }
      row.push_back(sum);
    }
    result._matrix.push_back(row);
  }
  return result;
}

FlexMatrix FlexMatrix::operator*(const FlexPoint &b) const {
  FlexMatrix result = *this;
  for (auto &row : result._matrix) {
    row = row * b;
  }
  return result;
}

FlexMatrix &FlexMatrix::operator+=(const FlexMatrix &b) {
  if (rows() != b.rows()) {
    throw std::invalid_argument("FlexMatrix: dimension mismatch");
  }
  for (size_t r = 0; r < rows(); r++) {
    _matrix[r] += b._matrix[r];
  }
  return *this;
}

FlexMatrix &FlexMatrix::operator-=(const FlexMatrix &b) {
  if (rows() != b.rows()) {
    throw std::invalid_argument("FlexMatrix: dimension mismatch");
  }
  for (size_t r = 0; r < rows(); r++) {
    _matrix[r] -= b._matrix[r];
  }
  return *this;
}

FlexMatrix &FlexMatrix::operator*=(const FlexMatrix &b) { return *this = *this * b; }

FlexMatrix FlexMatrix::quantized(const Precision &precision) const {
  FlexMatrix result = *this;
  for (auto &row : result._matrix) {
    row = row.quantized(precision);
  }
  return result;
}

bool FlexMatrix::operator==(const FlexMatrix &b) const {
  if (rows() != b.rows()) {
    return false;
  }
  for (size_t r = 0; r < rows(); r++) {
    if (!(_matrix[r] == b._matrix[r])) {
      return false;
    }
  }
  return true;
}

} // namespace Flex::Math
