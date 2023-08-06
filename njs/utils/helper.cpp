#include "helper.h"

#include <codecvt>
#include <sstream>
#include <iostream>
#include <cstdarg>
#include <cstdlib>
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
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.to_bytes(u16view.data(), u16view.data() + u16view.length());
}

std::string to_utf8_string(bool b) {
  return b ? "true" : "false";
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

int print_double_u16string(double val, char16_t *str) {
  double test_val;
  char output_buffer[40] = {0};
  int length;

  if (std::isnan(val) || std::isinf(val)) {
    length = sprintf(output_buffer, "null");
  }
  else {
     /* Try 15 decimal places of precision to avoid nonsignificant nonzero digits */
    length = sprintf(output_buffer, "%1.15g", val);
    /* Check whether the original double can be recovered */
    if ((sscanf(output_buffer, "%lg", &test_val) != 1) || !approximately_equal(test_val, val)) {
      /* If not, print with 17 decimal places of precision */
      length = sprintf(output_buffer, "%1.17g", val);
    }
  }

  for (int i = 0; i < length; i++) {
    *str = output_buffer[i];
    str += 1;
  }
  *str = 0;
   
  return length;
}

void print_red_line(const std::string& text) {
  const std::string RED_COLOR_CODE = "\033[1;31m";
  const std::string RESET_COLOR_CODE = "\033[0m";

  std::cout << RED_COLOR_CODE << text << RESET_COLOR_CODE << '\n';
}

bool approximately_equal(double a, double b) {
  return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool essentially_equal(double a, double b) {
  return fabs(a - b) <= ((fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool definitely_greater_than(double a, double b) {
  return (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool definitely_less_than(double a, double b) {
  return (b - a) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

}