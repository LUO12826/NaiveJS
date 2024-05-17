#include "conversion.h"

#include <cmath>
#include "double_to_str.h"
#include "njs/parser/character.h"
#include "njs/utils/lexing_helper.h"
#include "njs/utils/helper.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/JSString.h"
#include "njs/basic_types/JSBoolean.h"
#include "njs/basic_types/JSNumber.h"

namespace njs {

u16string double_to_u16string(double n) {
  char buf[JS_DTOA_BUF_SIZE];
  char16_t u16buf[JS_DTOA_BUF_SIZE];
  char *p_buf = buf;
  char16_t *p_u16buf = u16buf;

  js_dtoa(buf, n, 10, 0, JS_DTOA_VAR_FORMAT);
  while (*p_buf != '\0') {
    *p_u16buf++ = *p_buf++;
  }
  *p_u16buf = '\0';
  return u16string(u16buf, p_u16buf - u16buf);
}

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
        JSValue err = vm.build_error_internal(JS_TYPE_ERROR, u"cannot convert symbol to string");
        return Completion::with_throw(err);
      }
    case JSValue::BOOLEAN:
      return vm.get_string_const(val.val.as_bool ? AtomPool::k_true : AtomPool::k_false);
    case JSValue::NUM_UINT32:
      return vm.new_primitive_string(to_u16string(val.val.as_u32));
    case JSValue::NUM_INT32:
      return vm.new_primitive_string(to_u16string(val.val.as_i32));
    case JSValue::NUM_FLOAT: {
      u16string str = double_to_u16string(val.val.as_f64);
      return vm.new_primitive_string(std::move(str));
    }
    case JSValue::STRING:
      return val;
    default:
      if (val.is_object()) {
        Completion comp = val.as_object()->to_primitive(vm, u"string");
        if (comp.is_throw()) {
          return comp;
        } else {
          return js_to_string(vm, comp.get_value(), to_prop_key);
        }
      } else {
        assert(false);
      }
  }
}

Completion js_to_object(NjsVM &vm, JSValue arg) {
  if (arg.is_nil()) [[unlikely]] {
    return Completion::with_throw(vm.build_error_internal(JS_TYPE_ERROR, u""));
  }
  JSObject *obj;
  switch (arg.tag) {
    case JSValue::BOOLEAN:
      obj = vm.heap.new_object<JSBoolean>(vm, arg.val.as_bool);
      break;
    case JSValue::NUM_FLOAT:
      obj = vm.heap.new_object<JSNumber>(vm, arg.val.as_f64);
      break;
    case JSValue::STRING:
      obj = vm.heap.new_object<JSString>(vm, *arg.val.as_prim_string);
      break;
    default:
      if (arg.is_object()) return arg;
      else assert(false);
  }

  return JSValue(obj);
}

Completion js_to_property_key(NjsVM &vm, JSValue val) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
      return JSAtom(AtomPool::k_undefined);
    case JSValue::JS_NULL:
      return JSAtom(AtomPool::k_null);
    case JSValue::BOOLEAN:
      return JSAtom(val.val.as_bool ? AtomPool::k_true : AtomPool::k_false);
    case JSValue::JS_ATOM:
    case JSValue::SYMBOL:
      return val;
    case JSValue::NUM_UINT32:
      return JSAtom(vm.u32_to_atom(val.val.as_u32));
    case JSValue::NUM_FLOAT: {
      if (val.is_non_negative() && val.is_integer()) {
        auto int_idx = int64_t(val.val.as_f64);
        if (int_idx < UINT32_MAX) {
          return JSAtom(vm.u32_to_atom(int_idx));
        } else {
          u16string str = to_u16string(int_idx);
          return JSAtom(vm.str_to_atom_no_uint(str));
        }
      } else {
        u16string str = double_to_u16string(val.val.as_f64);
        return JSAtom(vm.str_to_atom_no_uint(str));
      }
    }
    case JSValue::STRING:
      return JSAtom(vm.str_to_atom(val.val.as_prim_string->str));
    default: {
      auto comp = js_to_string(vm, val, true);
      if (comp.is_throw()) return comp;
      return js_to_property_key(vm, comp.get_value());
    }
  }
}

double u16string_to_double(const u16string &str) {

  auto start = std::find_if_not(str.begin(), str.end(), [](char16_t ch) {
    return character::is_white_space(ch) || character::is_line_terminator(ch);
  });
  auto end = std::find_if_not(str.rbegin(), str.rend(), [](char16_t ch) {
               return character::is_white_space(ch) || character::is_line_terminator(ch);
             }).base();

  if (start >= end) {
    return 0;
  }

  bool positive = true;

  if (*start == u'-') {
    positive = false;
    start += 1;
  } else if (*start == u'+') {
    start += 1;
  }

  if (start >= end) return nan("");

  if (u16string(start, end) == u"Infinity") {
    return positive ? std::numeric_limits<double>::infinity()
                    : -std::numeric_limits<double>::infinity();
  }

  u32 cursor = start - str.begin();
  auto res = scan_numeric_literal(str.data(), str.size(), cursor);
  if (res.has_value() && cursor == str.size()) {
    return res.value();
  } else {
    return nan("");
  }
}

ErrorOr<double> js_to_number(NjsVM &vm, JSValue val) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
      return nan("");
    case JSValue::JS_NULL:
      return 0.0;
    case JSValue::BOOLEAN:
      return val.val.as_bool ? 1.0 : 0.0;
    case JSValue::NUM_FLOAT:
      return val.val.as_f64;
    case JSValue::SYMBOL:
      return vm.build_error_internal(u"TypeError");
    case JSValue::STRING:
      return u16string_to_double(val.val.as_prim_string->str);
    default:
      if (val.is_object()) {
        Completion comp = val.as_object()->to_primitive(vm, u"number");
        if (comp.is_throw()) return comp.get_value();

        return js_to_number(vm, comp.get_value());
      } else {
        assert(false);
      }
  }
  assert(false);
}


ErrorOr<u32> js_to_uint32(NjsVM &vm, JSValue val) {
  auto maybe_num = js_to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (x == 0.0 || std::isnan(x) || std::isinf(x)) {
    return 0;
  }
  // very simplified
  int64_t int_val = x;
  return u32(int_val);
}

ErrorOr<int32_t> js_to_int32(NjsVM &vm, JSValue val) {
  auto maybe_num = js_to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (x == 0.0 || std::isnan(x) || std::isinf(x)) {
    return 0;
  }
  // very simplified
  int64_t int_val = x;
  return int32_t(int_val);
}

}