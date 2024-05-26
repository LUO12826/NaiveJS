#ifndef NJS_STRING_H
#define NJS_STRING_H

#include <memory>
#include <cstring>
#include <string>
#include <string_view>
#include <cassert>
#include "njs/include/MurmurHash3.h"

namespace njs {

using u16string = std::u16string;
using u16string_view = std::u16string_view;

class String {
 private:
  std::unique_ptr<char16_t[]> ptr;
  size_t len;

 public:
  String() {
    len = 0;
    ptr = nullptr;
  }

  String(const char16_t *str) {
    if (str) {
      len = std::char_traits<char16_t>::length(str);
      ptr = std::make_unique<char16_t[]>(len);
      std::memcpy(ptr.get(), str, len * sizeof(char16_t));
    } else {
      len = 0;
      ptr = nullptr;
    }
  }

  String(const u16string& str) {
    len = str.size();
    ptr = std::make_unique<char16_t[]>(len);
    std::memcpy(ptr.get(), str.data(), len * sizeof(char16_t));
  }

  String(u16string_view str_view) {
    len = str_view.size();
    ptr = std::make_unique<char16_t[]>(len);
    std::memcpy(ptr.get(), str_view.data(), len * sizeof(char16_t));
  }

  String(const String &other) : len(other.len),
        ptr(std::make_unique<char16_t[]>(other.len)) {
    std::memcpy(ptr.get(), other.ptr.get(), len * sizeof(char16_t));
  }

  String(String &&other) noexcept: ptr(std::move(other.ptr)), len(other.len) {
    other.len = 0;
  }

  String &operator=(const String &other) {
    if (this != &other) {
      len = other.len;
      ptr = std::make_unique<char16_t[]>(len);
      std::memcpy(ptr.get(), other.ptr.get(), len * sizeof(char16_t));
    }
    return *this;
  }

  String &operator=(String &&other) noexcept {
    if (this != &other) {
      ptr = std::move(other.ptr);
      len = other.len;
      other.len = 0;
    }
    return *this;
  }


  ~String() = default;

  size_t size() const {
    return len;
  }

  const char16_t *data() const {
    return ptr.get();
  }

  u16string_view view() const {
    return {ptr.get(), len};
  }

  u16string to_std_string() const {
    return {ptr.get(), len};
  }

  char16_t operator[](size_t index) const {
    return ptr[index];
  }

  bool operator==(const String &other) const {
    if (len != other.len) return false;
    if (len == 0) return true;
    assert(ptr.get() && other.ptr.get());
    return std::memcmp(ptr.get(), other.ptr.get(), len * sizeof(char16_t)) == 0;
  }

  bool operator!=(const String &other) const {
    return !(*this == other);
  }

  bool operator>(const String &other) const {
    if (!ptr && !other.ptr) return false;
    if (!ptr) return false;
    if (!other.ptr) return true;
    size_t len_comp = std::min(len, other.len);
    return std::char_traits<char16_t>::compare(ptr.get(), other.ptr.get(), len_comp) > 0;
  }

  bool operator<(const String &other) const {
    if (!ptr && !other.ptr) return false;
    if (!ptr) return true;
    if (!other.ptr) return false;
    size_t len_comp = std::min(len, other.len);
    return std::char_traits<char16_t>::compare(ptr.get(), other.ptr.get(), len_comp) < 0;
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
