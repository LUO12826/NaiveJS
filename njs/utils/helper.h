#ifndef NJS_UTILS_HELPER_H
#define NJS_UTILS_HELPER_H

#include <string>
#include <string_view>


#include "njs/utils/macros.h"

void debug_printf(const char* format, ...);

namespace njs {

std::string to_utf8_string(std::u16string str);

std::string to_utf8_string(const std::u16string_view& u16view);

std::string to_utf8_string(bool b);

std::string to_utf8_string(const void *ptr);

std::u16string str_cat(std::vector<std::u16string> vals);


// From Knuth https://stackoverflow.com/a/253874/5163915
static constexpr double kEpsilon = 1e-15;
bool ApproximatelyEqual(double a, double b);

bool EssentiallyEqual(double a, double b);

bool DefinitelyGreaterThan(double a, double b);

bool DefinitelyLessThan(double a, double b);

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