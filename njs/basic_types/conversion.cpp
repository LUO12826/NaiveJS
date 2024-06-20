#include "conversion.h"

#include <cstdint>
#include <limits>
#include <cmath>
#include <cstdint>
#include "njs/basic_types/JSBoolean.h"
#include "njs/basic_types/JSNumber.h"
#include "njs/basic_types/JSString.h"
#include "njs/common/conversion_helper.h"
#include "njs/parser/character.h"
#include "njs/parser/lexing_helper.h"
#include "njs/utils/macros.h"
#include "njs/vm/NjsVM.h"

namespace njs {

Completion js_to_string(NjsVM &vm, JSValue val, bool to_prop_key) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
      return vm.get_string_const(AtomPool::k_undefined);
    case JSValue::JS_NULL:
      return vm.get_string_const(AtomPool::k_null);
    case JSValue::SYMBOL:
      if (to_prop_key) {
        return val;
      } else {
        return vm.throw_error(JS_TYPE_ERROR, u"cannot convert symbol to string");
      }
    case JSValue::BOOLEAN:
      return vm.get_string_const(val.as_bool ? AtomPool::k_true : AtomPool::k_false);
    case JSValue::NUM_UINT32:
      return vm.new_primitive_string(to_u16string(val.as_u32));
    case JSValue::NUM_INT32:
      return vm.new_primitive_string(to_u16string(val.as_i32));
    case JSValue::NUM_FLOAT: {
      u16string str = double_to_u16string(val.as_f64);
      return vm.new_primitive_string(str);
    }
    case JSValue::STRING:
      return val;
    default:
      if (val.is_object()) {
        JSValue prim = TRYCC(val.as_object->to_primitive(vm, HINT_STRING));
        return js_to_string(vm, prim, to_prop_key);
      } else {
        assert(false);
      }
  }
  __builtin_unreachable();
}

Completion js_to_object(NjsVM &vm, JSValue arg) {
  if (arg.is_nil()) [[unlikely]] {
    return vm.throw_error(JS_TYPE_ERROR, u"");
  }
  JSObject *obj;
  switch (arg.tag) {
    case JSValue::BOOLEAN:
      obj = vm.heap.new_object<JSBoolean>(vm, arg.as_bool);
      break;
    case JSValue::NUM_FLOAT:
      obj = vm.heap.new_object<JSNumber>(vm, arg.as_f64);
      break;
    case JSValue::STRING:
      obj = vm.heap.new_object<JSString>(vm, arg.as_prim_string);
      break;
    default:
      if (arg.is_object()) return arg;
      else assert(false);
  }

  return JSValue(obj);
}

Completion js_to_primitive(NjsVM &vm, JSValue val) {
  if (val.is_object()) [[likely]] {
    return val.as_object->to_primitive(vm);
  } else {
    return val;
  }
}

JSValue js_op_typeof(NjsVM &vm, JSValue val) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
    case JSValue::UNINIT:
      return vm.get_string_const(AtomPool::k_undefined);
    case JSValue::JS_NULL:
      return vm.get_string_const(AtomPool::k_object);
    case JSValue::JS_ATOM:
      return vm.get_string_const(AtomPool::k_string);
    case JSValue::SYMBOL:
      return vm.get_string_const(AtomPool::k_symbol);
    case JSValue::BOOLEAN:
      return vm.get_string_const(AtomPool::k_boolean);
    case JSValue::NUM_UINT32:
    case JSValue::NUM_INT32:
    case JSValue::NUM_FLOAT:
      return vm.get_string_const(AtomPool::k_number);
    case JSValue::STRING:
      return vm.get_string_const(AtomPool::k_string);
    case JSValue::BOOLEAN_OBJ:
    case JSValue::NUMBER_OBJ:
    case JSValue::STRING_OBJ:
    case JSValue::OBJECT:
    case JSValue::ARRAY:
      // will this happen?
      if (object_class(val) == CLS_FUNCTION) {
        return vm.get_string_const(AtomPool::k_function);
      } else {
        return vm.get_string_const(AtomPool::k_object);
      }
    case JSValue::FUNCTION:
      return vm.get_string_const(AtomPool::k_function);
    default:
      assert(false);
  }
  __builtin_unreachable();
}

Completion js_to_property_key(NjsVM &vm, JSValue val) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
      return JSAtom(AtomPool::k_undefined);
    case JSValue::JS_NULL:
      return JSAtom(AtomPool::k_null);
    case JSValue::BOOLEAN:
      return JSAtom(val.as_bool ? AtomPool::k_true : AtomPool::k_false);
    case JSValue::JS_ATOM:
    case JSValue::SYMBOL:
      return val;
    case JSValue::NUM_UINT32:
      return JSAtom(vm.u32_to_atom(val.as_u32));
    case JSValue::NUM_FLOAT: {
      if (val.is_non_negative() && val.is_integer()) {
        auto int_idx = int64_t(val.as_f64);
        if (int_idx < UINT32_MAX) {
          return JSAtom(vm.u32_to_atom(int_idx));
        } else {
          u16string str = to_u16string(int_idx);
          return JSAtom(vm.str_to_atom_no_uint(str));
        }
      } else {
        u16string str = double_to_u16string(val.as_f64);
        return JSAtom(vm.str_to_atom_no_uint(str));
      }
    }
    case JSValue::STRING:
      return JSAtom(vm.str_to_atom(val.as_prim_string->view()));
    default: {
      JSValue str = TRYCC(js_to_string(vm, val, true));
      assert(str.is_prim_string());
      return JSAtom(vm.str_to_atom(str.as_prim_string->view()));
    }
  }
}

Completion js_require_object_coercible(NjsVM &vm, JSValue val) {
  if (val.is_nil()) {
    return vm.throw_error(JS_TYPE_ERROR, u"undefined or null is not coercible");
  } else {
    return val;
  }
}

bool js_to_boolean(JSValue val) {
  return val.bool_value();
}

ErrorOr<double> js_to_number(NjsVM &vm, JSValue val) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
      return NAN;
    case JSValue::JS_NULL:
      return 0.0;
    case JSValue::BOOLEAN:
      return val.as_bool ? 1.0 : 0.0;
    case JSValue::NUM_FLOAT:
      return val.as_f64;
    case JSValue::SYMBOL:
      return vm.build_error(JS_TYPE_ERROR, u"TypeError");
    case JSValue::STRING:
      return u16string_to_double(val.as_prim_string->view());
    default:
      if (val.is_object()) {
        JSValue prim = TRY_ERR(val.as_object->to_primitive(vm, HINT_NUMBER));
        return js_to_number(vm, prim);
      } else {
        assert(false);
      }
  }
  assert(false);
  __builtin_unreachable();
}


ErrorOr<int64_t> js_to_int64sat(NjsVM &vm, JSValue val) {
  auto maybe_num = js_to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (std::isnan(x)) {
    return 0;
  } else if (x < INT64_MIN) {
    return INT64_MIN;
  } else if (x > INT64_MAX) {
    return INT64_MAX;
  } else {
    return x;
  }
}

ErrorOr<u32> js_to_uint32(NjsVM &vm, JSValue val) {
  auto maybe_num = js_to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (std::isnan(x) || std::isinf(x)) {
    return 0;
  }
  // TODO: may need to double check
  return (int64_t)x;
}

ErrorOr<int32_t> js_to_int32(NjsVM &vm, JSValue val) {
  auto maybe_num = js_to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (x == 0.0 || std::isnan(x) || std::isinf(x)) {
    return 0;
  }
  // TODO: may need to double check
  return (u32)(int64_t)x;
}

ErrorOr<uint16_t> js_to_uint16(NjsVM &vm, JSValue val) {
  auto maybe_num = js_to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (std::isnan(x) || std::isinf(x)) {
    return 0;
  }

  int64_t int_val = floor(fabs(x));
  return int_val;
}

ErrorOr<int16_t> js_to_int16(NjsVM &vm, JSValue val) {
  auto maybe_num = js_to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (std::isnan(x) || std::isinf(x)) {
    return 0;
  }

  int64_t int_val = floor(fabs(x));
  return int_val;
}

}