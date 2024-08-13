#ifndef NJS_CONVERSION_HELPER_H
#define NJS_CONVERSION_HELPER_H

#include <cmath>
#include <algorithm>
#include <codecvt>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include "njs/parser/character.h"
#include "double_to_str.h"
#include "njs/utils/helper.h"
#include "njs/parser/unicode.h"
#include "njs/parser/lexing_helper.h"

namespace njs {

using u32 = uint32_t;
using std::string;
using std::u16string;
using std::u16string_view;

inline u16string to_u16string(u32 value) {
  char16_t buffer[10];
  char16_t *pos = buffer + 10;

  do {
    *--pos = static_cast<char16_t>(u'0' + (value % 10));
    value /= 10;
  } while (value != 0);

  return u16string(pos, buffer + 10 - pos);
}

inline u16string to_u16string(int32_t value) {
  char16_t buffer[11];
  char16_t *pos = buffer + 11;
  int neg = value < 0;
  if (neg) value = -value;

  do {
    *--pos = static_cast<char16_t>(u'0' + ((u32)value % 10));
    value = (u32)value / 10;
  } while (value != 0);

  if (neg) { *--pos = u'-'; }

  return u16string(pos, buffer + 11 - pos);
}

inline u16string to_u16string(int64_t value) {
  char16_t buffer[20];
  char16_t *pos = buffer + 20;
  int neg = value < 0;
  if (neg) value = -value;

  do {
    *--pos = static_cast<char16_t>(u'0' + ((uint64_t)value % 10));
    value = (uint64_t)value / 10;
  } while (value != 0);

  if (neg) { *--pos = u'-'; }

  return u16string(pos, buffer + 20 - pos);
}

inline u16string to_u16string(const string& str) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.from_bytes(str);
}

inline u16string to_u16string(bool b) {
  return b ? u"true" : u"false";
}

inline string to_u8string(const u16string& str) {
  static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.to_bytes(str);
}

inline string to_u8string(const u16string_view u16view) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.to_bytes(u16view.data(), u16view.data() + u16view.length());
}

inline string to_u8string(bool b) {
  return b ? "true" : "false";
}

inline u16string str_cat(const std::vector<u16string>& vals) {
  u32 size = 0;
  for (const auto& val : vals) {
    size += val.size();
  }
  u16string res(size, 0);
  u32 offset = 0;
  for (auto val : vals) {
    memcpy((void *)(res.c_str() + offset), (void *)(val.data()), val.size() * 2);
    offset += val.size();
  }
  return res;
}

inline u16string to_escaped_u16string(u16string_view str_view) {
  u16string escaped;
  size_t escape_char_cnt = 0;

  for (auto ch : str_view) {
    switch (ch) {
      case '\"':
      case '\\':
      case '\b':
      case '\f':
      case '\n':
      case '\r':
      case '\t':
        escape_char_cnt += 1;
        break;
      default:
        if (ch < 32) {
          // UTF-16 escape sequence uXXXX
          escape_char_cnt += 5;
        }
        break;
    }
  }

  if (escape_char_cnt == 0) return u16string(str_view);

  escaped.resize(str_view.size() + escape_char_cnt);

  size_t output_ptr = 0;
  for (auto ch : str_view) {
    if (ch > 31 && ch != '\"' && ch != '\\') { escaped[output_ptr++] = ch; }
    else {
      escaped[output_ptr++] = '\\';
      switch (ch) {
        case '\\':
          escaped[output_ptr++] = '\\';
          break;
        case '\"':
          escaped[output_ptr++] = '\"';
          break;
        case '\b':
          escaped[output_ptr++] = 'b';
          break;
        case '\f':
          escaped[output_ptr++] = 'f';
          break;
        case '\n':
          escaped[output_ptr++] = 'n';
          break;
        case '\r':
          escaped[output_ptr++] = 'r';
          break;
        case '\t':
          escaped[output_ptr++] = 't';
          break;
        default:
          // escape and print as unicode codepoint
          char buf[10];
          sprintf(buf, "u%04x", (char)ch);
          for (int i = 0; i < 4; i++) {
            escaped[output_ptr++] = buf[i];
          }
          break;
      }
    }
  }

  return escaped;
}

inline void print_double_string(double n, char *str) {
  js_dtoa(str, n, 10, 0, JS_DTOA_VAR_FORMAT);
}

// for json stringify
inline void print_double_u16string(double n, char16_t *str) {
  char buf[JS_DTOA_BUF_SIZE];
  js_dtoa(buf, n, 10, 0, JS_DTOA_VAR_FORMAT);
  u8_to_u16_buffer(buf, str);
}

inline void json_double_u16string(double n, char16_t *str) {
  char buf[JS_DTOA_BUF_SIZE];
  if (std::isnan(n) || std::isinf(n)) [[unlikely]] {
    sprintf(buf, "null");
  } else {
    js_dtoa(buf, n, 10, 0, JS_DTOA_VAR_FORMAT);
  }
  u8_to_u16_buffer(buf, str);
}

inline u16string double_to_u16string(double n) {
  char buf[JS_DTOA_BUF_SIZE];
  js_dtoa(buf, n, 10, 0, JS_DTOA_VAR_FORMAT);
  
  char16_t u16buf[JS_DTOA_BUF_SIZE];
  u8_to_u16_buffer(buf, u16buf);
  return u16string(u16buf);
}

inline double parse_double(u16string_view str) {
  auto predicate = [] (char16_t ch) {
    return character::is_white_space(ch) || character::is_line_terminator(ch);
  };
  auto start = std::find_if_not(str.begin(), str.end(), predicate);
  auto end = std::find_if_not(str.rbegin(), str.rend(), predicate).base();

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

  if (start >= end) return NAN;

  if (u16string(start, end) == u"Infinity") {
    return positive ? std::numeric_limits<double>::infinity()
                    : -std::numeric_limits<double>::infinity();
  }

  u32 cursor = start - str.begin();
  auto res = scan_numeric_literal(str.data(), str.size(), cursor);
  if (res.has_value() && cursor == str.size()) {
    return positive ? res.value() : -res.value();
  } else {
    return NAN;
  }
}

inline double parse_int(u16string_view str) {
  auto predicate = [] (char16_t ch) {
    return character::is_white_space(ch) || character::is_line_terminator(ch);
  };
  auto start = std::find_if_not(str.begin(), str.end(), predicate);
  auto end = std::find_if_not(str.rbegin(), str.rend(), predicate).base();

  if (start >= end) {
    return NAN;
  }

  bool positive = true;
  int base = 10;

  if (*start == u'-') {
    positive = false;
    start += 1;
  } else if (*start == u'+') {
    start += 1;
  } else if (*start == u'0') {
    start += 1;
    if (start >= end) {
      return 0;
    }
    if (*start == u'x') {
      start += 1;
      base = 16;
    } else if (character::is_decimal_digit(*start)) {
      base = 10;
    } else {
      return 0;
    }
  }

  if (start >= end) return NAN;
  if (not character::is_hex_digit(*start)) {
    return 0;
  }

  u32 cursor = start - str.begin();
  auto res = scan_integer_literal(str.data(), str.size(), cursor, base);
  if (res.has_value()) {
    return positive ? res.value() : -res.value();
  } else {
    return NAN;
  }
}

}

#endif // NJS_CONVERSION_HELPER_H
