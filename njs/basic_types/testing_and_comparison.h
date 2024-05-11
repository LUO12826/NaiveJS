#ifndef NJS_TESTING_AND_COMPARISON_H
#define NJS_TESTING_AND_COMPARISON_H

#include "njs/vm/ErrorOr.h"
#include "njs/vm/Completion.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/JSValue.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/JSObject.h"

namespace njs {

inline ErrorOr<bool> strict_equals(NjsVM& vm, JSValue lhs, JSValue rhs) {
  bool res;
  if (lhs.tag == rhs.tag) {
    assert(lhs.tag != JSValue::HEAP_VAL && lhs.tag != JSValue::VALUE_HANDLE);
    assert(lhs.tag != JSValue::JS_ATOM);

    switch (lhs.tag) {
      case JSValue::BOOLEAN:
        res = lhs.val.as_bool == rhs.val.as_bool;
        break;
      case JSValue::NUM_FLOAT:
        res = lhs.val.as_f64 == rhs.val.as_f64;
        break;
      case JSValue::UNDEFINED:
      case JSValue::JS_NULL:
        res = true;
        break;
      case JSValue::STRING: {
        res = lhs.val.as_prim_string->str
              == rhs.val.as_prim_string->str;
        break;
      }
      default:
        if (lhs.is_object()) {
          res = lhs.val.as_object == rhs.val.as_object;
        } else {
          assert(false);
        }
    }
  }
  else {
    res = false;
  }

  return res;
}

inline ErrorOr<bool> abstract_equals(NjsVM& vm, JSValue lhs, JSValue rhs) {
  if (lhs.tag == rhs.tag) {
    return strict_equals(vm, lhs, rhs);
  }
  if (lhs.is_float64() && rhs.is_primitive_string()) {
    return lhs.val.as_f64 == u16string_to_double(rhs.val.as_prim_string->str);
  }
  else if (lhs.is_primitive_string() && rhs.is_float64()) {
    return rhs.val.as_f64 == u16string_to_double(lhs.val.as_prim_string->str);
  }
  else if (lhs.is_undefined() && rhs.is_null()) {
    return true;
  }
  else if (lhs.is_null() && rhs.is_undefined()) {
    return true;
  }
  else if (lhs.is_bool()) {
    double num_val = lhs.val.as_bool ? 1 : 0;
    return abstract_equals(vm, JSValue(num_val), rhs);
  }
  else if (rhs.is_bool()) {
    double num_val = rhs.val.as_bool ? 1 : 0;
    return abstract_equals(vm, lhs, JSValue(num_val));
  } else if ((lhs.is_float64() || lhs.is_primitive_string()) && rhs.is_object()) {
    Completion to_prim_res = rhs.as_object()->to_primitive(vm);

    if (to_prim_res.is_throw()) {
      return to_prim_res.get_value();
    } else {
      return abstract_equals(vm, lhs, to_prim_res.get_value());
    }
  } else if (lhs.is_object() && (rhs.is_float64() || rhs.is_primitive_string())) {
    Completion to_prim_res = lhs.as_object()->to_primitive(vm);
    if (to_prim_res.is_throw()) {
      return to_prim_res.get_value();
    } else {
      return abstract_equals(vm, to_prim_res.get_value(), rhs);
    }
  }

  return false;
}

}

#endif //NJS_TESTING_AND_COMPARISON_H
