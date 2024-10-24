#ifndef NJS_LEXING_HELPER_H
#define NJS_LEXING_HELPER_H

#include <cstdint>
#include <string>
#include <optional>
#include <cmath>
#include "njs/common/Defer.h"
#include "njs/parser/character.h"

namespace njs {

using std::u16string;
using std::u16string_view;
using std::optional;
using u32 = uint32_t;

// TODO: check this
inline char16_t escape_to_real_char(char16_t c) {
  switch (c) {
    case u'\'': return u'\'';
    case u'\\': return u'\\';
    case u'"': return u'"';
    case u'b': return u'\b';
    case u'f': return u'\f';
    case u'n': return u'\n';
    case u'r': return u'\r';
    case u't': return u'\t';
    case u'v': return u'\v';
    default:
      return c;
  }
}

// Scan index literal. Can only be decimal natural numbers in range [0, 4294967295].
// return -1 if failed.
inline int64_t scan_index_literal(u16string_view str) {
  if (str.empty() || str.size() > 10) return -1;
  u32 idx = 0;
  const char16_t *raw_str = str.data();
  char16_t ch = raw_str[idx];

  if (ch == u'0') return str.size() == 1 ? 0 : -1;

  int64_t int_val = 0;
  while (idx < str.size()) {
    ch = raw_str[idx];
    if (!character::is_decimal_digit(ch)) break;

    int_val = int_val * 10 + (ch - u'0');
    idx += 1;
  }
  return idx == str.size() ? int_val : -1;
}

// Scan decimal number. Only allows decimal digits.
inline optional<uint64_t> scan_decimal_literal(const char16_t *str, u32 str_len, u32& cursor) {
  u32 pos = cursor;
  char16_t ch = str[pos];

  if (!character::is_decimal_digit(ch)) {
    return std::nullopt;
  }
  uint64_t int_val = 0;
  while (character::is_decimal_digit(ch)) {
    int_val = int_val * 10 + character::u16_char_to_digit(ch);

    pos += 1;
    if (pos < str_len) ch = str[pos];
    else break;
  }
  cursor = pos;
  return int_val;
}

// Scan integer. Allows decimal digits and hexadecimal digits.
inline optional<uint64_t> scan_integer_literal(
    const char16_t *str, u32 str_len, u32& cursor, int base = 10) {
  u32 pos = cursor;
  char16_t ch = str[pos];

  if (!character::is_hex_digit(ch)) {
    return std::nullopt;
  }
  uint64_t int_val = 0;
  while (character::is_hex_digit(ch)) {
    int_val = int_val * base + character::u16_char_to_digit(ch);

    pos += 1;
    if (pos < str_len) ch = str[pos];
    else break;
  }
  cursor = pos;
  return int_val;
}

inline double scan_fractional_part(const char16_t *str, u32 str_len, u32& cursor) {
  u32 dec_start = cursor;
  auto scan_res = scan_decimal_literal(str, str_len, cursor);

  if (scan_res.has_value()) {
    u32 len = cursor - dec_start;
    return scan_res.value() / std::pow(10, len);
  }
  return 0;
}

#define NEXT_CHAR do { pos += 1; if (pos < str_len) ch = str[pos]; else ch = character::EOS; } while (0);
#define UPDATE_CHAR if (pos < str_len) ch = str[pos]; else { pos = str_len; ch = character::EOS; }

inline optional<double> scan_numeric_literal(const char16_t *str, u32 str_len, u32& cursor) {
  u32 pos = cursor;
  char16_t ch = str[pos];
  if (not (ch == u'.' || character::is_decimal_digit(ch))) {
    return std::nullopt;
  }

  uint64_t int_val = 0;
  double double_val = 0;

  bool is_hex = false;
  bool is_double = false;

  switch (ch) {
    case u'0': {
      NEXT_CHAR
      switch (ch) {
        case u'x':
        case u'X': {  // HexIntegerLiteral
          NEXT_CHAR
          auto scan_res = scan_integer_literal(str, str_len, pos, 16);
          UPDATE_CHAR
          if (!scan_res.has_value()) goto error;

          int_val = scan_res.value();
          is_hex = true;
          break;
        }
        case u'.': {
          is_double = true;
          NEXT_CHAR
          double_val = scan_fractional_part(str, str_len, pos);
          UPDATE_CHAR
          break;
        }
        case u'b':
        case u'B':
          // TODO: support for binary literal.
          break;
      }
      break;
    }
    case u'.': {
      is_double = true;
      NEXT_CHAR
      double_val = scan_fractional_part(str, str_len, pos);
      UPDATE_CHAR
      break;
    }
    default:  // NonZeroDigit
      auto scan_res = scan_decimal_literal(str, str_len, pos);
      UPDATE_CHAR
      if (!scan_res.has_value()) goto error;
      int_val = scan_res.value();

      if (ch == u'.') {
        is_double = true;
        NEXT_CHAR
        double_val = (double)int_val + scan_fractional_part(str, str_len, pos);
        UPDATE_CHAR
      }
  }

  if(!is_hex) {  // ExponentPart

    bool neg_exp = false;
    if (ch == u'e' || ch == u'E') {
      NEXT_CHAR
      if (ch == u'+' || ch == u'-') {
        neg_exp = ch == u'-';
        NEXT_CHAR
      }
      auto scan_res = scan_decimal_literal(str, str_len, pos);
      UPDATE_CHAR
      if (!scan_res.has_value()) goto error;

      double exp = neg_exp ? -double(scan_res.value()) : double(scan_res.value());
      if (is_double) {
        double_val = double_val * std::pow(10, exp);
      } else {
        is_double = true;
        double_val = double(int_val) * std::pow(10, exp);
      }
    }
  }

  // The source character immediately following a NumericLiteral must not
  // be an IdentifierStart or DecimalDigit.
  if (character::is_identifier_start(ch) || character::is_decimal_digit(ch)) {
    goto error;
  }
  cursor = pos;
  return is_double ? double_val : double(int_val);
error:
  cursor = pos;
  return std::nullopt;
}

inline bool scan_unicode_escape_sequence(const char16_t *str, u32 str_len,
                                            u32& cursor, std::u16string& res_str) {
  u32 pos = cursor;
  char16_t ch = str[pos];

  if (ch != u'u') {
    return false;
  }
  NEXT_CHAR
  char16_t c = 0;
  for (u32 i = 0; i < 4; i++) {
    if (!character::is_hex_digit(ch)) {
      cursor = pos;
      return false;
    }
    c = c << 4 | character::u16_char_to_digit(ch);
    NEXT_CHAR
  }
  res_str += c;
  cursor = pos;
  return true;
}

inline void skip_line_terminators(const char16_t *str,
                                  u32 str_len,
                                  u32& cursor,
                                  u32& curr_line,
                                  u32& curr_line_start) {
  u32 pos = cursor;
  defer {
    cursor = pos;
  };
  char16_t ch = str[pos];

  assert(character::is_line_terminator(ch));
  if (ch == character::LF) [[likely]] {
    NEXT_CHAR
  } else if (ch == character::CR && (pos + 1 < str_len) && str[pos + 1] == character::LF) {
    NEXT_CHAR
    NEXT_CHAR
  }
  curr_line += 1;
  curr_line_start = pos;
}

inline optional<u16string> scan_string_literal(const char16_t *str,
                                               u32 str_len,
                                               u32& cursor,
                                               u32& curr_line,
                                               u32& curr_line_start) {
  u32 pos = cursor;
  defer {
    cursor = pos;
  };
  char16_t ch = str[pos];
  char16_t quote = ch;
  std::u16string tmp;
  NEXT_CHAR

  while(pos != str_len && ch != quote && !character::is_line_terminator(ch)) {
    if (ch == u'\\') [[unlikely]] {
      NEXT_CHAR
      switch (ch) {
        case u'0': {
          NEXT_CHAR
          // TODO: disable octal escape sequence when in strict mode
          if (character::is_octal_digit(ch)) {
            u32 oct_value = ch - '0';
            NEXT_CHAR
            if (character::is_octal_digit(ch)) {
              oct_value = oct_value << 3 | (ch - '0');
              NEXT_CHAR
            }
            tmp += (char16_t)oct_value;
          } else {
            tmp += u'\0';
          }
          break;
        }
        case u'x': {  // HexEscapeSequence
          NEXT_CHAR
          char16_t c = 0;
          for (u32 i = 0; i < 2; i++) {
            if (!character::is_hex_digit(ch)) {
              goto error;
            }
            c = c << 4 | character::u16_char_to_digit(ch);
            NEXT_CHAR
          }
          tmp += c;
          break;
        }
        case u'u': {  // UnicodeEscapeSequence
          if (!scan_unicode_escape_sequence(str, str_len, pos, tmp)) {
            goto error;
          }
          UPDATE_CHAR
          break;
        }
        default:
          if (character::is_octal_digit(ch)) {
            u32 oct_value = ch - '0';
            NEXT_CHAR
            if (character::is_octal_digit(ch)) {
              oct_value = oct_value << 3 | (ch - '0');
              NEXT_CHAR
              if (oct_value < 32 && character::is_octal_digit(ch)) {
                oct_value = oct_value << 3 | (ch - '0');
                NEXT_CHAR
              }
            }
            tmp += (char16_t)oct_value;
          } else if (character::is_char_escape_sequence(ch)) {
            tmp += escape_to_real_char(ch);
            NEXT_CHAR
          } else if (character::is_line_terminator(ch)) {
            skip_line_terminators(str, str_len, pos, curr_line, curr_line_start);
            UPDATE_CHAR
          } else {
            tmp += ch;
            NEXT_CHAR
          }
      }
    }
    else {
      tmp += ch;
      NEXT_CHAR
    }
  }

  if (ch == quote) {
    NEXT_CHAR
    return tmp;
  }
error:
  NEXT_CHAR
  return std::nullopt;
}

}

#endif // NJS_LEXING_HELPER_H
