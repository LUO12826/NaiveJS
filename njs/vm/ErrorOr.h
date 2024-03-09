#ifndef NJS_ERROR_OR_H
#define NJS_ERROR_OR_H

#include <variant>
#include <string>
#include "njs/basic_types/JSValue.h"

namespace njs {


template<typename T>
class ErrorOr {

 public:
  ErrorOr() : value_or_err(T{}) {}

  ErrorOr(const T& value) : value_or_err(value) {}

  ErrorOr(const JSValue& error) : value_or_err(error) {}

  bool is_error() const {
    return std::holds_alternative<JSValue>(value_or_err);
  }

  bool is_value() const {
    return std::holds_alternative<T>(value_or_err);
  }

  T get_value() const {
    assert(not is_error());
    return std::get<T>(value_or_err);
  }

  JSValue get_error() const {
    assert(is_error());
    return std::get<JSValue>(value_or_err);
  }

 private:
  std::variant<T, JSValue> value_or_err;
};

}

#endif //NJS_ERROR_OR_H
