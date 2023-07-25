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

}