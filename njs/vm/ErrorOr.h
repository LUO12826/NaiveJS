#ifndef NJS_ERROR_OR_H
#define NJS_ERROR_OR_H

#include <variant>
#include <string>
#include "njs/basic_types/JSValue.h"

namespace njs {


template<typename T>
class ErrorOr {

 public:
  ErrorOr(const T& value) : valueOrError(value) {}

  ErrorOr(const JSValue& error) : valueOrError(error) {}

  bool is_error() const {
    return std::holds_alternative<JSValue>(valueOrError);
  }

  bool is_value() const {
    return std::holds_alternative<T>(valueOrError);
  }

  T get_value() const {
    assert(not is_error());
    return std::get<T>(valueOrError);
  }

  JSValue get_error() const {
    assert(is_error());
    return std::get<JSValue>(valueOrError);
  }

 private:
  std::variant<T, JSValue> valueOrError;
};

}

#endif //NJS_ERROR_OR_H
