#ifndef NJS_CONVERSION_H
#define NJS_CONVERSION_H

#include <string>
#include <algorithm>
#include <limits>
#include <cmath>
#include "njs/parser/character.h"
#include "njs/utils/lexing_helper.h"
#include "njs/vm/ErrorOr.h"
#include "njs/vm/NativeFunction.h"

using std::u16string;
using u32 = uint32_t;

namespace njs {
inline double u16string_to_double(const u16string &str) {

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
    return positive ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
  }

  u32 cursor = start - str.begin();
  auto res = scan_numeric_literal(str.data(), str.size(), cursor);
  if (res.has_value() && cursor == str.size()) {
    return res.value();
  } else {
    return nan("");
  }

}

inline ErrorOr<double> to_number(NjsVM &vm, JSValue val) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
      return nan("");
    case JSValue::JS_NULL:
      return 0.0;
    case JSValue::BOOLEAN:
      return val.val.as_bool ? 1.0 : 0.0;
    case JSValue::NUM_INT64:
      return double(val.val.as_i64);
    case JSValue::NUM_FLOAT:
      return val.val.as_f64;
    case JSValue::SYMBOL:
      return InternalFunctions::build_error_internal(vm, u"TypeError");
    case JSValue::STRING:
      return u16string_to_double(val.val.as_primitive_string->str);
    default:
      if (val.is_object()) {
        Completion comp = val.as_object()->to_primitive(vm, u"number");
        if (comp.is_throw()) return comp.get_value();

        return to_number(vm, comp.get_value());
      } else {
        assert(false);
      }
  }
  assert(false);
}

inline ErrorOr<u32> to_uint32(NjsVM &vm, JSValue val) {
  auto maybe_num = to_number(vm, val);
  if (maybe_num.is_error()) return maybe_num.get_error();

  double x = maybe_num.get_value();
  if (x == 0.0 || std::isnan(x) || std::isinf(x)) {
    return 0;
  }
  // very simplified
  int64_t int_val = x;
  return u32(int_val);
}

inline ErrorOr<int32_t> to_int32(NjsVM &vm, JSValue val) {
  auto maybe_num = to_number(vm, val);
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
#endif //NJS_CONVERSION_H
