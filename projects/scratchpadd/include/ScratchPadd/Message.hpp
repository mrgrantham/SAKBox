#pragma once

#include <array>
#include <iostream>
#include <variant>
#include <optional>
#include <vector>
#include <functional>
#include <unordered_map>
#include <string>
#include <spdlog/spdlog.h>
#include <ScratchPadd/Helper.hpp>

// This is the namespace for all data structures passed between padds
#define MessageTypes ScratchPadd::MessageType::Point, ScratchPadd::MessageType::Triangle, ScratchPadd::MessageType::Text, ScratchPadd::MessageType::Control

namespace ScratchPadd {
    struct BaseControlType {
      virtual void executeHandler()=0;
    };

    namespace ControlType {
      struct Double : public BaseControlType {
        double value;
        std::optional<std::pair<double,double>> enforcableRange;
        std::optional<std::function<void(double)>> handler;

        Double(double initialValue = 0.0, 
              std::optional<std::pair<double,double>> range = std::nullopt, 
              std::optional<std::function<void(double)>> controlHandler = std::nullopt) {
          value = initialValue;
          enforcableRange = range;
          handler = controlHandler;
        }

        void executeHandler() override {
          if (handler) {
            (*handler)(value);
          }
        }
      };

      struct Integer : public BaseControlType {
        int32_t value;
        std::optional<std::pair<int32_t,int32_t>> enforcableRange;
        std::optional<std::function<void(int32_t)>> handler;
        Integer(int32_t initialValue = 0.0,
                std::optional<std::pair<int32_t,int32_t>> range = std::nullopt, 
                std::optional<std::function<void(int32_t)>> controlHandler = std::nullopt) {
          value = initialValue;
          enforcableRange = range;
          handler = controlHandler;
        }
        void executeHandler() override {
          if (handler) {
            (*handler)(value);
          }
        }
      };

      struct Boolean : public BaseControlType {
        bool value;
        std::optional<std::function<void(bool)>> handler;
        Boolean(bool initialValue = 0.0, 
                std::optional<std::function<void(bool)>> controlHandler = std::nullopt) {
          value = initialValue;
          handler = controlHandler;
        }
        void executeHandler() override {
          if (handler) {
            (*handler)(value);
          }
        }
      };

      struct String : public BaseControlType {
        std::string value;
        std::optional<std::vector<std::string>> enforcableRange;
        std::optional<std::function<void(std::string)>> handler;
        String(std::string initialValue, 
                std::optional<std::vector<std::string>> range = std::nullopt, 
                std::optional<std::function<void(std::string)>> controlHandler = std::nullopt) {
          value = initialValue;
          enforcableRange = range;
          handler = controlHandler;
        }
        void executeHandler() override {
          if (handler) {
            (*handler)(value);
          }
        }
      };
    }
    #define ControlTypes ScratchPadd::ControlType::Double, ScratchPadd::ControlType::Integer, ScratchPadd::ControlType::Boolean, ScratchPadd::ControlType::String
    using ControlTypeVariant = std::variant<ControlTypes>;

  namespace MessageType {
    struct Control {
      std::string sourceName;
      std::unordered_map<std::string,ControlTypeVariant> controlMap;
    };

    namespace Transform {
      struct Translate {

      };
      struct Scale {

      };
      struct Rotate {

      };
    }

    struct Color {

    };
    struct Point {
      double x{0};
      double y{0};
      friend std::ostream &operator<<( std::ostream &output, const Point &p ) {
        output << "{ X:" << p.x << " Y: " << p.y << " } ";
        return output;
      }
    };

    struct Position : Point {};

    struct Triangle {
      Position position;
      std::array<Point,3> vertexes;
      friend std::ostream &operator<<( std::ostream &output, const Triangle &T ) {
        output << " { position:" << T.position  << ", vertexes: {\n" ;
        for (const Point &vertex: T.vertexes) {
          output << " " << vertex << " ,\n";
        }
        output << " }";
        return output;
      }
    };

    struct Text {
      std::string contents;
      friend std::ostream &operator<<( std::ostream &output, const Text &T ) {
        output << " { text context:" << T.contents << " }";
        return output;
      }
    };
  }

using MessageVariant = std::variant<MessageTypes>;
using Message = std::shared_ptr<MessageVariant>;

  template<class MessageType>
  static Message Make_Msg(MessageType &&messageType) {
    return std::make_shared<MessageVariant>(messageType);
  }
}