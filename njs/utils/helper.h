#ifndef NJS_UTILS_HELPER_H
#define NJS_UTILS_HELPER_H

#include <string>
#include <string_view>


#include "njs/utils/macros.h"

namespace njs {

using std::u16string;
using std::u16string_view;

void debug_printf(const char* format, ...);

std::u16string to_utf16_string(const std::string& str);

std::string to_utf8_string(const u16string& str);

std::string to_utf8_string(const u16string_view& u16view);

std::string to_utf8_string(bool b);

std::u16string str_cat(const std::vector<u16string>& vals);

int print_double_u16string(double val, char16_t *str);

// From Knuth https://stackoverflow.com/a/253874/5163915
static constexpr double kEpsilon = 1e-15;
bool approximately_equal(double a, double b);

bool essentially_equal(double a, double b);

bool definitely_greater_than(double a, double b);

bool definitely_less_than(double a, double b);

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