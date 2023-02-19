#pragma once
#include <vector>
#include <initializer_list>
#include <FlexPoint.hpp>

namespace Flex::Math {


class FlexVector {
    private:
    std::vector<FlexPoint> _vector;
    public:
    FlexVector(std::initializer_list<FlexPoint> vector);
    FlexVector();
    FlexVector operator+(const FlexVector &vector) const;
    FlexVector operator-(const FlexVector &vector) const;	
    FlexVector operator*(const FlexVector &vector) const;
    FlexVector operator*(const FlexPoint &vector) const;

    FlexVector& operator =(const FlexVector& vector);
    FlexVector& operator +=(const FlexVector& vector);
    FlexVector& operator -=(const FlexVector& vector);
    FlexVector& operator *=(const FlexVector& vector);

    auto operator<=>(const FlexVector &vector) = delete;
    auto operator==(const FlexVector &vector) const {
        assert(_vector.size() == vector._vector.size());

        for ( int i = 0; i < _vector.size(); i++) {
            if (this->_vector[i] != vector._vector[i])
                return false;
        }
        return true;
    }
};

}