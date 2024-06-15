#ifndef NJS_STRING_H
#define NJS_STRING_H

#include <algorithm>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <string>
#include <codecvt>
#include <locale>
#include <string_view>
#include "njs/include/MurmurHash3.h"

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

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

  struct Data {
    uint64_t w1;
    uint64_t w2;
    uint64_t w3;
    uint64_t w4;
  };

  union {
    Short inline_str;
    Long heap_str;
    Data _data;
  };

  static constexpr size_t capacity_reserve(size_t cap) {
    return cap + (cap >> 2);
  }

  void init_data(const char16_t *str, size_t length) {
    assert(str != nullptr);
    char16_t *data_start;
    if (length <= SHORT_CAPACITY) {
      inline_str.len = length;
      data_start = inline_str.data;
    }
    else {
      assert(length <= LONG_STR_LEN_MAX);
      inline_str.len = LONG_STR_MAGIC;
      heap_str.len = length;
      heap_str.cap = capacity_reserve(length + 1);
      heap_str.data = new char16_t[heap_str.cap];
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

  const char16_t* get_data() const {
    if (is_inline()) {
      assert(inline_str.len <= SHORT_CAPACITY);
      return inline_str.data;
    } else {
      return heap_str.data;
    }
  }

  String concatenate(const char16_t* str, size_t length) const {
    String result;
    size_t total_length = get_length() + length;

    if (total_length <= SHORT_CAPACITY) {
      result.inline_str.len = total_length;
      std::memcpy(result.inline_str.data, get_data(), get_length() * CHAR_SIZE);
      std::memcpy(result.inline_str.data + get_length(), str, length * CHAR_SIZE);
      result.inline_str.data[total_length] = 0;
    } else {
      result.inline_str.len = LONG_STR_MAGIC;
      result.heap_str.len = total_length;
      result.heap_str.cap = capacity_reserve(total_length + 1);
      result.heap_str.data = new char16_t[result.heap_str.cap];
      std::memcpy(result.heap_str.data, get_data(), get_length() * CHAR_SIZE);
      std::memcpy(result.heap_str.data + get_length(), str, length * CHAR_SIZE);
      result.heap_str.data[total_length] = 0;
    }

    return result;
  }

  void append_data(const char16_t* str, size_t length) {
    size_t this_length = get_length();
    size_t new_length = this_length + length;

    if (new_length <= SHORT_CAPACITY) {
      std::memcpy(inline_str.data + this_length, str, length * CHAR_SIZE);
      inline_str.len = new_length;
      inline_str.data[new_length] = 0;
    } else {
      if (is_inline()) {
        size_t new_cap = capacity_reserve(new_length + 1);
        auto *new_data = new char16_t[new_cap];
        std::memcpy(new_data, inline_str.data, this_length * CHAR_SIZE);
        std::memcpy(new_data + this_length, str, length * CHAR_SIZE);
        new_data[new_length] = 0;

        heap_str.len = new_length;
        heap_str.cap = new_cap;
        heap_str.data = new_data;
        inline_str.len = LONG_STR_MAGIC;
      } else {
        if (heap_str.cap < new_length + 1) {
          size_t new_cap = (new_length + 1) + ((new_length + 1) >> 1);
          auto *new_data = new char16_t[new_cap];
          std::memcpy(new_data, heap_str.data, this_length * CHAR_SIZE);
          delete[] heap_str.data;
          heap_str.data = new_data;
          heap_str.cap = new_cap;
        }
        std::memcpy(heap_str.data + this_length, str, length * CHAR_SIZE);
        heap_str.data[new_length] = 0;
        heap_str.len = new_length;
      }
    }
  }

 public:
  static constexpr size_t npos = SIZE_MAX;

  String() {
    init_empty();
  }

  String(char16_t ch) {
    inline_str.len = 1;
    inline_str.data[0] = ch;
    inline_str.data[1] = 0;
  }

  String(const char16_t *str) {
    if (str) {
      size_t length = char16_traits::length(str);
      init_data(str, length);
    } else {
      init_empty();
    }
  }

  String(const char16_t *str, size_t length) {
    if (str) {
      init_data(str, length);
    } else {
      init_empty();
    }
  }

  explicit String(const u16string& str) {
    init_data(str.data(), str.size());
  }

  explicit String(u16string_view str_view) {
    init_data(str_view.data(), str_view.size());
  }

  String(const String &other) {
    init_data(other.get_data(), other.get_length());
  }

  String(String &&other) noexcept {
    _data.w1 = other._data.w1;
    _data.w2 = other._data.w2;
    _data.w3 = other._data.w3;
    _data.w4 = other._data.w4;

    other.inline_str.len = 0;
    other.inline_str.data[0] = 0;
  }

  ~String() {
    if (is_on_heap()) {
      delete[] heap_str.data;
    }
  }

  String& operator=(const String &other) {
    if (this == &other) [[unlikely]] {
      return *this;
    }
    if (is_on_heap()) {
      delete[] heap_str.data;
    }
    _data.w1 = other._data.w1;
    _data.w2 = other._data.w2;
    _data.w3 = other._data.w3;
    _data.w4 = other._data.w4;

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
    _data.w1 = other._data.w1;
    _data.w2 = other._data.w2;
    _data.w3 = other._data.w3;
    _data.w4 = other._data.w4;

    other.inline_str.len = 0;
    other.inline_str.data[0] = 0;
    return *this;
  }

 public:
  void resize(size_t new_length) {
    size_t curr_length = get_length();

    if (new_length <= SHORT_CAPACITY) {
      if (is_inline()) {
        if (new_length > curr_length) {
          std::memset(inline_str.data + curr_length, 0, (new_length - curr_length) * CHAR_SIZE);
        }
        inline_str.len = new_length;
        inline_str.data[new_length] = 0;
      } else {
        // must have new_length < curr_length
        auto old_data = heap_str.data;
        std::memcpy(inline_str.data, old_data, new_length * CHAR_SIZE);
        delete[] old_data;
        inline_str.len = new_length;
        inline_str.data[new_length] = 0;
      }
    }
    // new_length > SHORT_CAPACITY
    else {
      if (is_inline()) {
        auto* new_data = new char16_t[new_length + 1];
        std::memcpy(new_data, inline_str.data, curr_length * CHAR_SIZE);
        std::memset(new_data + curr_length, 0, (new_length - curr_length) * CHAR_SIZE);
        heap_str.len = new_length;
        heap_str.cap = new_length + 1;
        heap_str.data = new_data;
        inline_str.len = LONG_STR_MAGIC;
      } else {
        if (heap_str.cap < new_length + 1) {
          auto* new_data = new char16_t[new_length + 1];
          std::memcpy(new_data, heap_str.data, curr_length * CHAR_SIZE);
          delete[] heap_str.data;
          heap_str.data = new_data;
          heap_str.cap = new_length + 1;
        }
        if (new_length > curr_length) {
          std::memset(heap_str.data + curr_length, 0, (new_length - curr_length) * CHAR_SIZE);
        }
        heap_str.len = new_length;
        heap_str.data[new_length] = 0;
      }
    }
  }


  String substr(size_t pos = 0, size_t len = npos) const {
    if (pos > get_length()) {
      throw std::out_of_range("String::substr: pos out of range");
    }
    size_t actual_len = std::min(len, get_length() - pos);

    if (actual_len == 0) [[unlikely]] {
      return String();
    }
    return String(get_data() + pos, actual_len);
  }

  size_t find(char16_t ch, size_t pos = 0) const {
    auto data = get_data();
    auto len = get_length();
    if (pos >= len) return npos;
    auto res = char16_traits::find(data + pos, len - pos, ch);
    return likely(res) ? res - data : npos;
  }

  size_t find(const String& pattern, size_t pos = 0) const {
    return find(pattern.data(), pattern.size(), pos);
  }

  size_t find(const char16_t* str, size_t length, size_t pos = 0) const {
    size_t this_length = get_length();

    if (!str || pos >= get_length()) [[unlikely]] {
      return npos;
    }
    if (length == 0) [[unlikely]] {
      return pos; // If the search string is empty, return the starting position
    }
    if (length > this_length - pos) [[unlikely]] {
      return npos; // If the search string is longer than the remaining string, return npos
    }

    const char16_t* data_start = get_data() + pos;
    const char16_t* data_end = get_data() + this_length;
    const std::boyer_moore_searcher searcher(str, str + length);
    const auto it = std::search(data_start, data_end, searcher);

    if (it != data_end) {
      return it - get_data();
    } else {
      return npos;
    }
  }

  size_t rfind(const String& pattern, size_t pos = 0) const {
    return rfind(pattern.data(), pattern.size(), pos);
  }

  size_t rfind(const char16_t* str, size_t length, size_t pos = 0) const {
    assert(str);
    size_t this_length = get_length();
    pos = std::min(pos, this_length);

    if (length == 0) [[unlikely]] {
      return pos; // If the search string is empty, return the starting position
    }

    const char16_t* data_start = get_data();
    const char16_t* data_end = get_data() + std::min(pos + length, this_length);
    const auto it = std::find_end(data_start, data_end, str, str + length);

    if (it != data_end) {
      return it - get_data();
    } else {
      return npos;
    }
  }

  void replace(size_t pos, size_t len, const String& replacement) {
    size_t curr_length = get_length();
    size_t rep_length = replacement.get_length();

    if (pos > curr_length) [[unlikely]] {
      throw std::out_of_range("String::replace: pos out of range");
    }

    size_t end_pos = pos + len;
    if (end_pos > curr_length) [[unlikely]] {
      len = curr_length - pos;
      end_pos = curr_length;
    }
    size_t new_length = curr_length - len + rep_length;
    resize(std::max(curr_length, new_length));
    auto data = get_data();

    memmove(data + pos + rep_length, data + end_pos, (curr_length - end_pos) * CHAR_SIZE);
    memcpy(data + pos, replacement.get_data(), rep_length * CHAR_SIZE);
    resize(new_length);
  }

  void replace(size_t pos, size_t len, const char16_t* replacement) {
    replace(pos, len, String(replacement));
  }

  void replace(size_t pos, size_t len, const char16_t* replacement, size_t replace_length) {
    replace(pos, len, String(replacement, replace_length));
  }


  size_t find(u16string_view sv, size_t pos = 0) const {
    return find(sv.data(), sv.size(), pos);
  }

  size_t find(const char16_t* str, size_t pos = 0) const {
    return find(str, char16_traits::length(str), pos);
  }

  size_t find(const u16string& str, size_t pos = 0) const {
    return find(str.data(), str.size(), pos);
  }

  size_t size() const {
    return get_length();
  }

  bool empty() const {
    return is_empty();
  }

  char16_t* data() {
    return get_data();
  }

  const char16_t* data() const {
    return get_data();
  }

  char16_t* begin() {
    return get_data();
  }

  char16_t* end() {
    return get_data() + get_length();
  }

  u16string_view view() const {
    return {get_data(), get_length()};
  }

  u16string to_std_u16string() const {
    return {get_data(), get_length()};
  }

  std::string to_std_u8string() {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(get_data(), get_data() + get_length());
  }

  void append(const String& other) {
    append_data(other.get_data(), other.get_length());
  }

  void append(const char16_t* str) {
    if (!str) return;
    append_data(str, char16_traits::length(str));
  }

  void append(const char16_t* str, size_t length) {
    if (!str) return;
    append_data(str, length);
  }

  void append(char16_t ch) {
    char16_t buf[2] {ch, 0};
    append_data(buf, 1);
  }

  void append(const u16string& other) {
    append_data(other.data(), other.size());
  }

  void append(u16string_view sv) {
    append_data(sv.data(), sv.size());
  }

  String operator+(const String& other) const {
    return concatenate(other.get_data(), other.get_length());
  }

  String operator+(const char16_t* str) const {
    return concatenate(str, char16_traits::length(str));
  }

  friend String operator+(const char16_t* lhs, const String& rhs) {
    String result(lhs);
    result.append_data(rhs.get_data(), rhs.get_length());
    return result;
  }

  friend String operator+(u16string_view sv, const String& rhs) {
    String result(sv);
    result.append_data(rhs.get_data(), rhs.get_length());
    return result;
  }

  String& operator+=(const String& other) {
    append(other);
    return *this;
  }

  String& operator+=(const char16_t* str) {
    append(str);
    return *this;
  }

  String& operator+=(const u16string& other) {
    append(other);
    return *this;
  }

  String& operator+=(u16string_view sv) {
    append(sv);
    return *this;
  }

  String& operator+=(char16_t ch) {
    append(ch);
    return *this;
  }

  char16_t operator[](size_t index) const {
    return get_data()[index];
  }

  bool operator==(const String &other) const {
    if (get_length() != other.get_length()) return false;
    int comp = std::memcmp(get_data(), other.get_data(), get_length() * CHAR_SIZE);
    return comp == 0;
  }

  bool operator!=(const String &other) const {
    return !(*this == other);
  }

  bool operator>(const String &other) const {
    if (is_empty() && other.is_empty()) return false;
    if (other.is_empty()) return true;
    size_t len_comp = std::min(get_length(), other.get_length());
    return char16_traits::compare(get_data(), other.get_data(), len_comp) > 0;
  }

  bool operator<(const String &other) const {
    if (is_empty() && other.is_empty()) return false;
    if (is_empty()) return true;
    if (other.is_empty()) return false;
    size_t len_comp = std::min(get_length(), other.get_length());
    return char16_traits::compare(get_data(), other.get_data(), len_comp) < 0;
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
    return std::hash<u16string_view>{}(s.view());
  }
};

}  // namespace std


#endif // NJS_STRING_H
