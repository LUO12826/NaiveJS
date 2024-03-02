#ifndef NJS_UTILS_HELPER_H
#define NJS_UTILS_HELPER_H

#include <string>
#include <string_view>
#include <codecvt>
#include <sstream>
#include <iostream>
#include <cstdarg>
#include <cstdlib>
#include <cctype>
#include <algorithm>

namespace njs {

using u32 = uint32_t;
using std::string;
using std::u16string;
using std::u16string_view;

// From Knuth https://stackoverflow.com/a/253874/5163915
static constexpr double EPS = 1e-15;

inline bool approximately_equal(double a, double b) {
  return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * EPS);
}

inline bool essentially_equal(double a, double b) {
  return fabs(a - b) <= ((fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * EPS);
}

inline bool definitely_greater_than(double a, double b) {
  return (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * EPS);
}

inline bool definitely_less_than(double a, double b) {
  return (b - a) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * EPS);
}

inline void debug_printf(const char* format, ...) {

#ifdef DBGPRINT
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif

  va_list args;
  va_start(args, format);
  DEBUG_PRINT(format, args);
  va_end(args);
}

inline u16string to_u16string(const string& str) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.from_bytes(str);
}

inline string to_u8string(const u16string &str) {
  static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.to_bytes(str);
}

inline string to_u8string(const u16string_view &u16view) {
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
    memcpy((void*)(res.c_str() + offset), (void*)(val.data()), val.size() * 2);
    offset += val.size();
  }
  return res;
}

inline u16string to_escaped_u16string(const u16string &str) {
  u16string escaped;
  size_t escape_char_cnt = 0;

  for (auto ch : str) {
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

  if (escape_char_cnt == 0) return str;

  escaped.resize(str.size() + escape_char_cnt);

  size_t output_ptr = 0;
  for (auto ch : str) {
    if (ch > 31 && ch != '\"' && ch != '\\') {
      escaped[output_ptr++] = ch;
    }
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

inline int print_double_string(double val, char *str) {
  double test_val;
  int length;

  if (std::isnan(val) || std::isinf(val)) {
    length = sprintf(str, "null");
  }
  else {
    /* Try 15 decimal places of precision to avoid nonsignificant nonzero digits */
    length = sprintf(str, "%1.15g", val);
    /* Check whether the original double can be recovered */
    if ((sscanf(str, "%lg", &test_val) != 1) || !approximately_equal(test_val, val)) {
      /* If not, print with 17 decimal places of precision */
      length = sprintf(str, "%1.17g", val);
    }
  }

  return length;
}

inline int print_double_u16string(double val, char16_t *str) {
  char output_buffer[40] = {0};
  int length = print_double_string(val, output_buffer);
  // Convert to utf16 string
  for (int i = 0; i < length; i++) {
    *str = output_buffer[i];
    str += 1;
  }
  *str = 0;

  return length;
}

inline void print_red_line(const string& text) {
  const string RED_COLOR_CODE = "\033[1;31m";
  const string RESET_COLOR_CODE = "\033[0m";

  std::cout << RED_COLOR_CODE << text << RESET_COLOR_CODE << '\n';
}

inline u16string trim(const u16string& str) {
  auto start = std::find_if_not(str.begin(), str.end(), [](char16_t ch) {
    return std::isspace(ch);
  });
  auto end = std::find_if_not(str.rbegin(), str.rend(), [](char16_t ch) {
    return std::isspace(ch);
  }).base();

  if (start >= end) {
    return u"";
  }

  return u16string(start, end);
}


template <typename T>
inline void hash_combine(size_t& seed, const T& val) {
  seed ^= std::hash<T>()(val) + 0X9E3779B9 + (seed << 6) + (seed >> 2);
}

template <typename T>
inline void hash_val(size_t& seed, const T& val) {
  hash_combine(seed, val);
}

template <typename T, typename... Types>
inline void hash_val(size_t& seed, const T& val, const Types&... args) {
  hash_combine(seed, val);
  hash_val(seed, args...);
}

template <typename... Types>
inline size_t hash_val(const Types&... args) {
  size_t seed = 0;
  hash_val(seed, args...);
  return seed;
}

}  // namespace njs



#endif  // NJS_UTILS_HELPER_H