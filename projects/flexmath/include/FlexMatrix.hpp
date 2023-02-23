#pragma once

#include <FlexVector.hpp>
namespace Flex::Math {

using Scalar = int32_t;

class FlexMatrix {
private:
  std::vector<FlexVector> _matrix;

public:
  FlexMatrix(std::initializer_list<FlexVector> matrix);
  FlexMatrix();

  FlexMatrix operator+(const FlexMatrix &b) const;
  FlexMatrix operator-(const FlexMatrix &b) const;
  FlexMatrix operator*(const FlexMatrix &b) const;
  FlexMatrix operator*(const FlexPoint &b) const;

  FlexMatrix &operator=(const FlexMatrix &b);
  FlexMatrix &operator+=(const FlexMatrix &b);
  FlexMatrix &operator-=(const FlexMatrix &b);
  FlexMatrix &operator*=(const FlexMatrix &b);
  // bool operator <=>(const FlexMatrix &b) const;

  bool operator==(const FlexMatrix &b) const;
  bool operator!=(const FlexMatrix &b) const;
  bool operator<(const FlexMatrix &b) const;
  bool operator>(const FlexMatrix &b) const;
  bool operator<=(const FlexMatrix &b) const;
  bool operator>=(const FlexMatrix &b) const;
};

} // namespace Flex::Math