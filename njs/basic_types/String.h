#ifndef NJS_STRING_H
#define NJS_STRING_H

#include <memory>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <string>
#include <string_view>
#include <cassert>
#include "njs/include/MurmurHash3.h"

namespace njs {

using u16string = std::u16string;
using u16string_view = std::u16string_view;
using char16_traits = std::char_traits<char16_t>;

class String {
 private:
  static constexpr uint8_t SHORT_CAPACITY = 14;
  static constexpr uint8_t LONG_STR_MAGIC = 0x80;
  static constexpr size_t LONG_STR_LEN_MAX = (SIZE_MAX >> 8);
  static constexpr int CHAR_SIZE = sizeof(char16_t);

  struct Long {
    uint8_t pad;
    size_t len : 56;
    size_t cap;
    char16_t *data;
  };

  struct Short {
    union {
      uint8_t len;
      uint8_t pad;
    };
    char16_t data[SHORT_CAPACITY + 1];
  };

  union {
    Short inline_str;
    Long heap_str;
  };

  void init_data(const char16_t *str, size_t length) {
    char16_t *data_start;
    if (length <= SHORT_CAPACITY) {
      inline_str.len = length;
      data_start = inline_str.data;
    }
    else {
      assert(length <= LONG_STR_LEN_MAX);
      inline_str.len = LONG_STR_MAGIC;
      heap_str.len = length;
      heap_str.cap = length + 1;
      heap_str.data = new char16_t[length + 1];
      data_start = heap_str.data;
    }
    std::memcpy(data_start, str, length * CHAR_SIZE);
    data_start[length] = 0;
  }

  void init_empty() {
    inline_str.len = 0;
    inline_str.data[0] = 0;
  }

  size_t get_length() const {
    if (is_inline()) {
      assert(inline_str.len <= SHORT_CAPACITY);
      return inline_str.len;
    } else {
      return heap_str.len;
    }
  }

  bool is_empty() const {
    return inline_str.len == 0;
  }

  bool is_inline() const {
    return inline_str.len != LONG_STR_MAGIC;
  }

  bool is_on_heap() const {
    return inline_str.len == LONG_STR_MAGIC;
  }

  char16_t* get_data() {
    if (is_inline()) {
      assert(inline_str.len <= SHORT_CAPACITY);
      return inline_str.data;
    } else {
      return heap_str.data;
    }
  }

  const char16_t* get_data_const() const {
    if (is_inline()) {
      assert(inline_str.len <= SHORT_CAPACITY);
      return inline_str.data;
    } else {
      return heap_str.data;
    }
  }

 public:
  String() {
    init_empty();
  }

  String(const char16_t *str) {
    if (str) {
      size_t length = char16_traits::length(str);
      init_data(str, length);
    } else {
      init_empty();
    }
  }

  String(const u16string& str) {
    init_data(str.data(), str.size());
  }

  String(u16string_view str_view) {
    init_data(str_view.data(), str_view.size());
  }

  String(const String &other) {
    init_data(other.get_data_const(), other.get_length());
  }

  String(String &&other) noexcept {
    heap_str.pad = other.heap_str.pad;
    heap_str.len = other.heap_str.len;
    heap_str.cap = other.heap_str.cap;
    heap_str.data = other.heap_str.data;
    
    other.inline_str.len = 0;
    other.inline_str.data[0] = 0;
  }

  String& operator=(const String &other) {
    if (this == &other) [[unlikely]] {
      return *this;
    }
    if (is_on_heap()) {
      delete[] heap_str.data;
    }
    heap_str.pad = other.heap_str.pad;
    heap_str.len = other.heap_str.len;
    heap_str.cap = other.heap_str.cap;
    heap_str.data = other.heap_str.data;

    if (other.is_on_heap()) {
      heap_str.data = new char16_t[heap_str.cap];
      memcpy(heap_str.data, other.heap_str.data, heap_str.len * CHAR_SIZE);
      heap_str.data[heap_str.len] = 0;
    }
    return *this;
  }

  String& operator=(String &&other) noexcept {
    if (this == &other) [[unlikely]] {
      return *this;
    }
    if (is_on_heap()) {
      delete[] heap_str.data;
    }
    heap_str.pad = other.heap_str.pad;
    heap_str.len = other.heap_str.len;
    heap_str.cap = other.heap_str.cap;
    heap_str.data = other.heap_str.data;

    other.inline_str.len = 0;
    other.inline_str.data[0] = 0;
    return *this;
  }


  ~String() {
    if (is_on_heap()) {
      delete[] heap_str.data;
    }
  }

  size_t size() const {
    return get_length();
  }

  const char16_t *data() const {
    return get_data_const();
  }

  u16string_view view() const {
    return {get_data_const(), get_length()};
  }

  u16string to_std_string() const {
    return {get_data_const(), get_length()};
  }

  char16_t operator[](size_t index) const {
    return get_data_const()[index];
  }

  bool operator==(const String &other) const {
    if (get_length() != other.get_length()) return true;
    int comp = std::memcmp(get_data_const(), other.get_data_const(), get_length() * CHAR_SIZE);
    return comp == 0;
  }

  bool operator!=(const String &other) const {
    return !(*this == other);
  }

  bool operator>(const String &other) const {
    if (is_empty() && other.is_empty()) return false;
    if (other.is_empty()) return true;
    size_t len_comp = std::min(get_length(), other.get_length());
    return char16_traits::compare(get_data_const(), other.get_data_const(), len_comp) > 0;
  }

  bool operator<(const String &other) const {
    if (is_empty() && other.is_empty()) return false;
    if (is_empty()) return true;
    if (other.is_empty()) return false;
    size_t len_comp = std::min(get_length(), other.get_length());
    return char16_traits::compare(get_data_const(), other.get_data_const(), len_comp) < 0;
  }

  bool operator>=(const String &other) const {
    return !(*this < other);
  }

  bool operator<=(const String &other) const {
    return !(*this > other);
  }
};

} // namespace njs

namespace std {

template <>
struct hash<njs::String> {
  size_t operator()(const njs::String& s) const noexcept {
    uint64_t output[2];
    MurmurHash3_x64_128(s.data(), s.size() * sizeof(char16_t), 31, &output);
    return output[0] ^ output[1];
  }
};

}  // namespace std


#endif // NJS_STRING_H
