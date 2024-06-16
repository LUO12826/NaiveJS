#ifndef NJS_COMPLETION_H
#define NJS_COMPLETION_H

#include <optional>
#include "njs/basic_types/JSValue.h"

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

  Completion(): type(Type::NORMAL) {}
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

  bool is_error() {
    return type == Type::THROW;
  }

  JSValue& get_value() {
    return val_or_err;
  }

  JSValue& get_error() {
    return val_or_err;
  }

 private:
  Type type;
  JSValue val_or_err;
};

inline Completion CompThrow(JSValue err) {
  return {Completion::Type::THROW, err};
}

}



#endif //NJS_COMPLETION_H
