#ifndef NJS_TESTING_AND_COMPARISON_H
#define NJS_TESTING_AND_COMPARISON_H

#include <cmath>
#include "njs/common/ErrorOr.h"
#include "njs/common/Completion.h"
#include "njs/basic_types/JSValue.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/JSObject.h"
#include "njs/common/conversion_helper.h"

namespace njs {

inline bool strict_equals(NjsVM& vm, JSValue lhs, JSValue rhs) {
  if (lhs.tag == rhs.tag) {
    assert(lhs.tag != JSValue::HEAP_VAL && lhs.tag != JSValue::VALUE_HANDLE);
    assert(lhs.tag != JSValue::JS_ATOM);

    switch (lhs.tag) {
      case JSValue::BOOLEAN:
        return lhs.as_bool == rhs.as_bool;
      case JSValue::NUM_FLOAT:
        return lhs.as_f64 == rhs.as_f64;
      case JSValue::UNDEFINED:
      case JSValue::JS_NULL:
        return true;
      case JSValue::STRING:
        return *lhs.as_prim_string == *rhs.as_prim_string;
      default:
        if (lhs.is_object()) {
          return lhs.as_object == rhs.as_object;
        } else {
          assert(false);
          __builtin_unreachable();
        }
    }
  }
  else {
    return false;
  }
}

inline ErrorOr<bool> abstract_equals(NjsVM& vm, JSValue lhs, JSValue rhs) {
  if (lhs.tag == rhs.tag) {
    return strict_equals(vm, lhs, rhs);
  }
  if (lhs.is_float64() && rhs.is_prim_string()) {
    return lhs.as_f64 == parse_double(rhs.as_prim_string->view());
  }
  else if (lhs.is_prim_string() && rhs.is_float64()) {
    return rhs.as_f64 == parse_double(lhs.as_prim_string->view());
  }
  else if (lhs.is_nil() && rhs.is_nil()) {
    return true;
  }
  else if (lhs.is_bool()) {
    double num_val = lhs.as_bool ? 1 : 0;
    return abstract_equals(vm, JSValue(num_val), rhs);
  }
  else if (rhs.is_bool()) {
    double num_val = rhs.as_bool ? 1 : 0;
    return abstract_equals(vm, lhs, JSValue(num_val));
  }
  else if ((lhs.is_float64() || lhs.is_prim_string()) && rhs.is_object()) {
    JSValue to_prim_res = TRY_ERR(rhs.as_object->to_primitive(vm));
    return abstract_equals(vm, lhs, to_prim_res);
  }
  else if (lhs.is_object() && (rhs.is_float64() || rhs.is_prim_string())) {
    JSValue to_prim_res = TRY_ERR(lhs.as_object->to_primitive(vm));
    return abstract_equals(vm, to_prim_res, rhs);
  }

  return false;
}

inline bool same_number_value(double lhs, double rhs) {
  if (lhs != rhs) {
    return std::isnan(lhs) && std::isnan(rhs);
  } else {
    return std::signbit(lhs) == std::signbit(rhs);
  }
}

inline bool same_value(JSValue lhs, JSValue rhs) {
  if (lhs.tag != rhs.tag) return false;
  if (lhs.is_undefined() || lhs.is_null()) return true;
  switch (lhs.tag) {
    case JSValue::NUM_FLOAT:
      return same_number_value(lhs.as_f64, lhs.as_f64);
    case JSValue::BOOLEAN:
      return lhs.as_bool == rhs.as_bool;
    case JSValue::STRING:
      return *lhs.as_prim_string == *rhs.as_prim_string;
    case JSValue::SYMBOL:
      return lhs.as_symbol == rhs.as_symbol;
    default:
      assert(lhs.is_object());
      return lhs.as_object == rhs.as_object;
  }
}

}

#endif //NJS_TESTING_AND_COMPARISON_H
