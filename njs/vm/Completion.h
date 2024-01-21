#ifndef NJS_COMPLETION_H
#define NJS_COMPLETION_H

#include "njs/basic_types/JSValue.h"
#include <optional>

using std::optional;

namespace njs {

class Completion {

 public:
  enum class Type: int8_t {
    EMPTY,
    NORMAL,
    BREAK,
    CONTINUE,
    RETURN,
    THROW,
  };

  static Completion with_throw(JSValue err) {
    return Completion(Type::THROW, err);
  }

  Completion(JSValue val): type(Type::NORMAL), val_or_err(val) {}
  Completion(Type ty, JSValue val): type(ty), val_or_err(val) {}

  Type get_type() {
    return type;
  }

  bool is_normal() {
    return type == Type::NORMAL;
  }

  bool is_throw() {
    return type == Type::THROW;
  }

  optional<JSValue> get_value() {
    return val_or_err;
  }

  JSValue get_value_or_undefined() {
    return val_or_err.value_or(JSValue::undefined);
  }

  JSValue get_error() {
    assert(type == Type::THROW && val_or_err.has_value());
    return val_or_err.value();
  }

 private:
  Type type;
  optional<JSValue> val_or_err;
};

}



#endif //NJS_COMPLETION_H
