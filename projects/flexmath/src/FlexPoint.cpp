#include "FlexPoint.hpp"

namespace Flex::Math {

FlexPoint& FlexPoint::operator =(const FlexPoint& value) {
    this->flexGroupID_ = value.flexGroupID_;
    this->fractionalComponent_ = value.fractionalComponent_;
    this->valueFixed_ = value.valueFixed_;
    return *this;
}

FlexPoint& FlexPoint::operator =(const double& value) {
    this->fractionalComponent_ = FlexController::getInstance()->getActiveFlexGroup()->getFractionalComponent();
    this->valueFixed_ = convertDoubleToFixed(value);
    return *this;
}

FlexPoint& FlexPoint::operator +=(const FlexPoint& value) {
    this->flexGroupID_ = value.flexGroupID_;
    this->fractionalComponent_ = value.fractionalComponent_;
    this->valueFixed_ = value.valueFixed_;
    return *this;
}

FlexPoint& FlexPoint::operator -=(const FlexPoint& value) {
    this->flexGroupID_ = value.flexGroupID_;
    this->fractionalComponent_ = value.fractionalComponent_;
    this->valueFixed_ = value.valueFixed_;
    return *this;
}

FlexPoint& FlexPoint::operator *=(const FlexPoint& value) {
    this->flexGroupID_ = value.flexGroupID_;
    this->fractionalComponent_ = value.fractionalComponent_;
    this->valueFixed_ = value.valueFixed_;
    return *this;
}

}
