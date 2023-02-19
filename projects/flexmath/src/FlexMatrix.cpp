#include "FlexMatrix.hpp"
namespace Flex::Math {

  FlexMatrix::FlexMatrix(std::initializer_list<FlexVector> matrix) : _matrix(matrix) {

  }

  FlexMatrix::FlexMatrix() {
    
  }

  FlexMatrix FlexMatrix::operator+(const FlexMatrix &b) const{
    return FlexMatrix();
  }
  FlexMatrix FlexMatrix::operator-(const FlexMatrix &b) const {
    return FlexMatrix();
  }
  FlexMatrix FlexMatrix::operator*(const FlexMatrix &b) const {
    return FlexMatrix();
  }
  FlexMatrix FlexMatrix::operator*(const FlexPoint &b) const {
    return FlexMatrix();
  }

  FlexMatrix& FlexMatrix::operator =(const FlexMatrix& b) {
    return *this;
  }
  FlexMatrix& FlexMatrix::operator +=(const FlexMatrix& b) {
    return *this;
  }
  FlexMatrix& FlexMatrix::operator -=(const FlexMatrix& b) {
    return *this;
  }
  FlexMatrix& FlexMatrix::operator *=(const FlexMatrix& b) {
    return *this;

  }
  // bool FlexMatrix::operator <=>(const FlexMatrix &b) const {
  //   return false;
  // }

  bool FlexMatrix::operator ==(const FlexMatrix &b) const {
    // FIXME: Finish Implemetation
    return true;
  }

  bool FlexMatrix::operator !=(const FlexMatrix &b) const {
    // FIXME: Finish Implemetation
    return true;
  }

  bool FlexMatrix::operator <(const FlexMatrix &b) const {
    // FIXME: Finish Implemetation
    return true;
  }

  bool FlexMatrix::operator >(const FlexMatrix &b) const {
    // FIXME: Finish Implemetation
    return true;
  }

  bool FlexMatrix::operator <=(const FlexMatrix &b) const {
    // FIXME: Finish Implemetation
    return true;
  }

  bool FlexMatrix::operator >=(const FlexMatrix &b) const {
    // FIXME: Finish Implemetation
    return true;
  }
}