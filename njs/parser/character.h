#ifndef NJS_CHARACTER_H
#define NJS_CHARACTER_H

#include "njs/parser/unicode.h"
#include "njs/utils/helper.h"

namespace njs {
namespace character {

using u32 = uint32_t;

// End of Line
static const char16_t EOS = 0x0000;

// Format-Control Character
static const char16_t ZWNJ = 0x200C;  // Zero width non-joiner
static const char16_t ZWJ  = 0x200D;  // Zero width joiner
static const char16_t BOM  = 0xFEFF;  // Byte Order Mark

// White Space
static const char16_t TAB     = 0x0009;  // Tab
static const char16_t VT      = 0x000B;  // Vertical Tab
static const char16_t FF      = 0x000C;  // Form Feed
static const char16_t SP      = 0x0020;  // Space
static const char16_t hashx0a = 0x00A0;  // No-break space
// static const char16_t BOM  = 0xFEFF;  // Byte Order Mark
// USP includes lots of characters, therefore only included in the function.

// Line Terminators
static const char16_t LF = 0x000A;
static const char16_t CR = 0x000D;
static const char16_t LS = 0x2028;
static const char16_t PS = 0x2029;


inline bool is_unicode_separator(char16_t c) {
  return c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
         c == 0x202F || c == 0x205F || c == 0x3000;
}

inline bool is_white_space(char16_t c) {
  return c == TAB || c == VT || c == FF ||
         c == FF  || c == SP || c == hashx0a ||
         is_unicode_separator(c);
}

inline bool is_line_terminator(char16_t c) {
  return c == LF || c == CR || c == LS || c == PS;
}

inline bool is_decimal_digit(char16_t c) {
  return c >= u'0' && c <= u'9';
}

inline bool is_unicode_letter(char16_t c) {
  return ((1 << get_unicode_category(c)) & (Lu | Ll | Lt | Lm | Lo | Nl));
}

inline bool is_unicode_combining_mark(char16_t c) {
  return ((1 << get_unicode_category(c)) & (Mn | Mc));
}

inline bool is_unicode_digit(char16_t c) {
  return get_unicode_category(c) == DECIMAL_DIGIT_NUMBER;
}

inline bool is_unicode_connector_punctuation(char16_t c) {
  return get_unicode_category(c) == CONNECTOR_PUNCTUATION;
}

inline bool is_hex_digit(char16_t c) {
  return is_decimal_digit(c) || (u'A' <= c && c <= u'F') || (u'a' <= c && c <= u'f');
}

inline bool is_radix_digit(char16_t c, int radix) {
  if (radix == 10)
    return is_decimal_digit(c);
  else if (radix < 10)
    return u'0' <= c && c < u'0' + radix;
  else
    return is_decimal_digit(c) || (u'A' <= c && c <= u'A' + radix - 10) || (u'a' <= c && c <= u'a' + radix - 10);
}

inline bool is_single_escape_char(char16_t c) {
  return c == u'\'' || c == u'"' || c == u'\\' || c == u'b' ||
         c == u'f'  || c == u'f' || c == u'n'  || c == u'r' ||
         c == u't'  || c == u'v';
}

//  TODO: 这里想干啥，decimal_digit也是转义符号？
inline bool is_escape_character(char16_t c) {
  return is_single_escape_char(c) || is_decimal_digit(c) ||
         c == u'x' || c == u'u';
}

inline bool is_nonescape_char(char16_t c) {
  return !is_escape_character(c) && !is_line_terminator(c);
}

// TODO: 这里到底想干啥
inline bool is_char_escape_sequence(char16_t c) {
  // NOTE(zhuzilin) The chars that are not in { LineTerminator, DecimalDigit, u'x', u'u' }.
  return is_single_escape_char(c) || is_nonescape_char(c);
}

inline bool is_identifier_start(char16_t c) {
  return is_unicode_letter(c) || c == u'$' || c == u'_' || c == u'\\';
}

inline bool legal_identifier_char(char16_t c) {
  return is_identifier_start(c) || is_unicode_combining_mark(c) ||
         is_unicode_digit(c) || is_unicode_connector_punctuation(c) ||
         c == ZWNJ || c == ZWJ;
}

inline bool is_regular_expression_char(char16_t c) {
  return !is_line_terminator(c) && c != u'\\' && c != u'/' && c != u'[';
}

inline bool is_regular_expression_first_char(char16_t c) {
  return !is_line_terminator(c) && c != u'*' && c != u'/';
}

inline bool is_regular_expression_class_char(char16_t c) {
  return !is_line_terminator(c) && c != u']';
}

inline u32 u16_char_to_digit(char16_t c) {

  if (u'0' <= c && c <= u'9')
    return c - u'0';
  if (u'A' <= c && c <= u'Z')
    return c - u'A' + 10;
  if (u'a' <= c && c <= u'z')
    return c - u'a' + 10;
  assert(false);
  
  __builtin_unreachable();
}

inline char16_t to_lower_case(char16_t c) {
  if ('A' <= c && c <= 'Z') {
    return c + ('a' - 'A');
  }
  // lowercase not found until 192
  if (c < 192) {
    return c;
  }
  // suppress compiler warnings
  {
    const u32 index = c - 192;
    if (index < k_lower_case_cache.size()) {
      ASSERT(index < k_lower_case_cache.size());
      return k_upper_case_cache[index];
    }
  }
  std::array<char16_t, 101>::const_iterator it =
        std::upper_bound(k_lower_case_keys.begin(), k_lower_case_keys.end(), c) - 1;
  
  const int result = static_cast<int>(it - k_lower_case_keys.begin());
  ASSERT(result < 101);
  if (result >= 0) {
    bool by2 = false;
    const char16_t start = k_lower_case_keys[result];
    char16_t end = k_lower_case_values[result * 2];
    if ((start & 0x8000) != (end & 0x8000)) {
        end ^= 0x8000;
        by2 = true;
    }
    if (c <= end) {
      if (by2 && (c & 1) != (start & 1)) {
        return c;
      }
      const char16_t mapping = k_lower_case_values[result * 2 + 1];
      return c + mapping;
    }
  }
  return c;
}

inline char16_t to_upper_case(char16_t c) {
  if ('a' <= c && c <= 'z') {
    return c - ('a' - 'A');
  }
  // uppercase not found until 181
  if (c < 181) {
    return c;
  }

  // suppress compiler warnings
  {
    const u32 index = c - 181;
    if (index < k_upper_case_cache.size()) {
      ASSERT(index < k_upper_case_cache.size());
      return k_upper_case_cache[index];
    }
  }
  std::array<char16_t, 113>::const_iterator it =
          std::upper_bound(k_upper_case_keys.begin(), k_upper_case_keys.end(), c) - 1;

  const int result = static_cast<int>(it - k_upper_case_keys.begin());
  ASSERT(result < 113);

  if (result >= 0) {
    bool by2 = false;
    const char16_t start = *it;
    char16_t end = k_upper_case_values[result * 2];
    if ((start & 0x8000) != (end & 0x8000)) {
      end ^= 0x8000;
      by2 = true;
    }
    if (c <= end) {
      if (by2 && (c & 1) != (start & 1)) {
        return c;
      }
      const char16_t mapping = k_upper_case_values[result * 2 + 1];
      return c + mapping;
    }
  }
  return c;
}

}  // namespace character
}  // namespace njs

#endif  // NJS_CHARACTER_H