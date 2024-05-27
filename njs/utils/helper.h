#ifndef NJS_UTILS_HELPER_H
#define NJS_UTILS_HELPER_H

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

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

inline void debug_printf(const char *format, ...) {

#ifdef DBGPRINT
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)                                                                           \
  do {                                                                                             \
  } while (0)
#endif

  va_list args;
  va_start(args, format);
  DEBUG_PRINT(format, args);
  va_end(args);
}

inline void print_red_line(const string& text) {
  const string RED_COLOR_CODE = "\033[1;31m";
  const string RESET_COLOR_CODE = "\033[0m";

  std::cout << RED_COLOR_CODE << text << RESET_COLOR_CODE << '\n';
}

inline u16string trim(const u16string& str) {
  auto start =
      std::find_if_not(str.begin(), str.end(), [](char16_t ch) { return std::isspace(ch); });
  auto end = std::find_if_not(str.rbegin(), str.rend(), [](char16_t ch) {
               return std::isspace(ch);
             }).base();

  if (start >= end) { return u""; }

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
inline void hash_val(size_t& seed, const T& val, const Types&...args) {
  hash_combine(seed, val);
  hash_val(seed, args...);
}

template <typename... Types>
inline size_t hash_val(const Types&...args) {
  size_t seed = 0;
  hash_val(seed, args...);
  return seed;
}

} // namespace njs

#endif // NJS_UTILS_HELPER_H