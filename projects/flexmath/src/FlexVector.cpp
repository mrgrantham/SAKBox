#include <FlexVector.hpp>
#include <assert.h> 

namespace Flex::Math {

    FlexVector::FlexVector(std::initializer_list<FlexPoint> vector) : _vector(vector) {

    }

    FlexVector::FlexVector() {

    }

    FlexVector FlexVector::operator+( const FlexVector &vector) const {
        assert(_vector.size() == vector._vector.size());
        FlexVector flexVector;

        for ( int i = 0; i < _vector.size(); i++) {
            flexVector._vector.emplace_back(_vector[i] + vector._vector[i]);
        }
        return flexVector;
    }

    FlexVector FlexVector::operator-(const FlexVector &vector) const {
        assert(_vector.size() == vector._vector.size());
        FlexVector flexVector;

        for ( int i = 0; i < _vector.size(); i++) {
            flexVector._vector.emplace_back(_vector[i] - vector._vector[i]);
        }
        return flexVector;
    }

    FlexVector FlexVector::operator*(const FlexVector &vector) const {
        assert(_vector.size() == vector._vector.size());
        FlexVector flexVector;

        for ( int i = 0; i < _vector.size(); i++) {
            flexVector._vector.emplace_back(_vector[i] * vector._vector[i]);
        }
        return flexVector;
    }

    FlexVector FlexVector::operator*(const FlexPoint &value) const {
        FlexVector flexVector;

        for ( int i = 0; i < _vector.size(); i++) {
            flexVector._vector.emplace_back(_vector[i] * value);
        }
        return flexVector;
    }


    FlexVector& FlexVector::operator=(const FlexVector& vector) {
        FlexVector flexVector;
        for (const auto &value : vector._vector) {
            flexVector._vector.push_back(value);
        }
        return *this;
    }

    FlexVector& FlexVector::operator+=(const FlexVector& vector) {
        assert(_vector.size() == vector._vector.size());

        for ( int i = 0; i < _vector.size(); i++) {
            this->_vector[i] += vector._vector[i];
        }
        return *this;
    }

    FlexVector& FlexVector::operator-=(const FlexVector& vector) {
        assert(_vector.size() == vector._vector.size());

        for ( int i = 0; i < _vector.size(); i++) {
            this->_vector[i] -= vector._vector[i];
        }
        return *this;
    }

    FlexVector& FlexVector::operator*=(const FlexVector& vector) {
        assert(_vector.size() == vector._vector.size());

        for ( int i = 0; i < _vector.size(); i++) {
            this->_vector[i] *= vector._vector[i];
        }
        return *this;
    }

}