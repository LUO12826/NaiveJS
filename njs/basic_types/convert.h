#ifndef NJS_CONVERT_H
#define NJS_CONVERT_H

#include <string>
#include <algorithm>
#include <limits>
#include "njs/parser/character.h"
#include "njs/utils/lexing_helper.h"

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

}

#endif //NJS_CONVERT_H
