#include "helper.h"

#include <codecvt>
#include <sstream>
#include <cstdarg>
#include "njs/parser/character.h"

#ifdef DBGPRINT
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif

namespace njs {

void debug_printf(const char* format, ...) {
  va_list args;
  va_start(args, format);

  DEBUG_PRINT(format, args);

  va_end(args);
}

std::u16string to_utf16_string(const std::string& str) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.from_bytes(str);
}

std::string to_utf8_string(const std::u16string& str) {
  static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.to_bytes(str);
}

std::string to_utf8_string(const std::u16string_view& u16view) {
  std::u16string u16str(u16view);
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.to_bytes(u16str);
}

std::string to_utf8_string(bool b) {
  return b ? "true" : "false";
}

std::string to_utf8_string(const void *ptr) {
  std::stringstream ss;
  ss << ptr;  
  return ss.str();
}

std::u16string str_cat(const std::vector<std::u16string>& vals) {
  u32 size = 0;
  for (const auto& val : vals) {
    size += val.size();
  }
  std::u16string res(size, 0);
  u32 offset = 0;
  for (auto val : vals) {
    memcpy((void*)(res.c_str() + offset), (void*)(val.data()), val.size() * 2);
    offset += val.size();
  }
  return res;
}

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

bool ApproximatelyEqual(double a, double b) {
  return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool EssentiallyEqual(double a, double b) {
  return fabs(a - b) <= ((fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool DefinitelyGreaterThan(double a, double b) {
  return (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool DefinitelyLessThan(double a, double b) {
  return (b - a) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

}