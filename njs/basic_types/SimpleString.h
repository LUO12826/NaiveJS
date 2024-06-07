#ifndef NJS_SIMPLE_STRING_H
#define NJS_SIMPLE_STRING_H

#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace njs {

using std::u16string;
using std::u16string_view;

class SimpleString {
 private:
  char16_t *data;
  size_t len;

 public:
  SimpleString() : data(nullptr), len(0) {}

  SimpleString(const u16string& str) : len(str.size()) {
    data = new char16_t[len];
    std::memcpy(data, str.data(), len * sizeof(char16_t));
  }

  SimpleString(u16string_view str) : len(str.size()) {
    data = new char16_t[len];
    std::memcpy(data, str.data(), len * sizeof(char16_t));
  }

  SimpleString(const SimpleString& other) : len(other.len) {
    data = new char16_t[len];
    std::memcpy(data, other.data, len * sizeof(char16_t));
  }

  SimpleString(SimpleString&& other) noexcept
    : data(other.data), len(other.len) {
    other.data = nullptr;
    other.len = 0;
  }

  SimpleString& operator=(const SimpleString& other) {
    if (this == &other) { return *this; }
    delete[] data;
    len = other.len;
    data = new char16_t[len];
    std::memcpy(data, other.data, len * sizeof(char16_t));
    return *this;
  }

  SimpleString& operator=(SimpleString&& other) noexcept {
    if (this == &other) { return *this; }
    delete[] data;
    data = other.data;
    len = other.len;
    other.data = nullptr;
    other.len = 0;
    return *this;
  }

  ~SimpleString() {
    delete[] data;
  }

  size_t size() const {
    return len;
  }

  const char16_t *c_str() const {
    return data;
  }

  const u16string_view view() const {
    return {data, len};
  }
};

} // namespace njs

#endif // NJS_SIMPLE_STRING_H
