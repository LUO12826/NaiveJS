#include "lexing_helper.h"

#include "njs/parser/character.h"

namespace njs {

int64_t scan_index_literal(const u16string& str) {
  u32 idx = 0;
  const char16_t *raw_str = str.data();

  char16_t ch = raw_str[idx];
  if (ch == u'0') return str.size() == 1 ? 0 : -1;

  if (!character::is_decimal_digit(ch)) {
    return -1;
  }

  int64_t int_val = 0;
  while (idx < str.size()) {
    ch = raw_str[idx];
    if (!character::is_decimal_digit(ch)) break;

    int_val = int_val * 10 + character::u16_char_to_digit(ch);
    idx += 1;
  }
  return idx == str.size() ? int_val : -1;
}

optional<uint64_t> scan_decimal_literal(const char16_t *str, u32 str_len, u32& cursor) {
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

optional<uint64_t> scan_integer_literal(const char16_t *str, u32 str_len, u32& cursor, int base) {
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

double scan_fractional_part(const char16_t *str, u32 str_len, u32& cursor) {
  u32 dec_start = cursor;
  auto scan_res = scan_decimal_literal(str, str_len, cursor);

  if (scan_res.has_value()) {
    u32 len = cursor - dec_start;
    return scan_res.value() / std::pow(10, len);
  }
  return 0;
}

#define NEXT_CHAR do { pos += 1; if (pos < str_len) ch = str[pos]; else ch = character::EOS; } while (0);

optional<double> scan_numeric_literal(const char16_t *str, u32 str_len, u32& cursor) {
  u32 pos = cursor;
  char16_t ch = str[pos];

  assert(ch == u'.' || character::is_decimal_digit(ch));
  u32 start = cursor;

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
          auto scan_res = scan_integer_literal(str, str_len, pos);
          if (!scan_res.has_value()) goto error;

          int_val = scan_res.value();
          is_hex = true;
          break;
        }
        case u'.': {
          is_double = true;
          NEXT_CHAR
          double_val = scan_fractional_part(str, str_len, pos);
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
      break;
    }
    default:  // NonZeroDigit
      auto scan_res = scan_decimal_literal(str, str_len, pos);
      if (!scan_res.has_value()) goto error;
      int_val = scan_res.value();

      if (ch == u'.') {
        is_double = true;
        NEXT_CHAR
        double_val = (double)int_val + scan_fractional_part(str, str_len, pos);
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

  return is_double ? double_val : double(int_val);
error:
  cursor = pos;
  return std::nullopt;
}

}