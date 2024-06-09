#ifndef NJS_PRIMITIVE_STRING_H
#define NJS_PRIMITIVE_STRING_H

#include <string>
#include "njs/gc/GCObject.h"
#include "njs/gc/GCHeap.h"
#include "njs/common/conversion_helper.h"
#include "njs/common/common_def.h"

namespace njs {

using std::u16string;
using char16_traits = std::char_traits<char16_t>;

struct PrimitiveString: public GCObject {

friend class GCHeap;

  static inline uint64_t concat_count {0};
  static inline uint64_t alloc_count {0};
  static inline uint64_t concat_length {0};
  static inline uint64_t fast_concat_count {0};

  static constexpr u32 npos = UINT32_MAX;

  PrimitiveString(const PrimitiveString& other) = delete;
  PrimitiveString(PrimitiveString&& other) = delete;
  PrimitiveString& operator=(PrimitiveString&& other) = delete;
  PrimitiveString& operator=(const PrimitiveString& other) = delete;

  std::string description() override {
    return "PrimitiveString(" + to_std_string() + ")";
  }

  u16string_view view() const {
    return {storage, (size_t)len};
  }

  u16string to_std_u16string() const {
    return {storage, (size_t)len};
  }

  string to_std_string() const {
    return to_u8string(view());
  }

  PrimitiveString* concat(GCHeap& heap, PrimitiveString *str) {
    return concat(heap, str->storage, str->len);
  }

  PrimitiveString* concat(GCHeap& heap, const char16_t* str, u32 length) {
    u32 new_length = len + length;
    assert(new_length < UINT32_MAX);

    concat_count += 1;
    concat_length += new_length;

    // This optimization doesn't look very effective
//    PrimitiveString *new_str;
//    if (not referenced && new_length <= cap) {
//      fast_concat_count += 1;
//      new_str = this;
//    } else {
//      new_str = heap.new_prim_string(new_length);
//      std::memcpy(new_str->storage, storage, len * CHAR_SIZE);
//    }

    PrimitiveString *new_str = heap.new_prim_string(new_length);
    std::memcpy(new_str->storage, storage, len * CHAR_SIZE);
    std::memcpy(new_str->storage + len, str, length * CHAR_SIZE);

    new_str->storage[new_length] = 0;
    new_str->len = new_length;
    return new_str;
  }

  PrimitiveString* substr(GCHeap& heap, u32 pos, u32 length = npos) const {
    pos = std::min(pos, len);
    length = std::min(length, len - pos);

    PrimitiveString *sub_str = heap.new_prim_string(length);
    std::memcpy(sub_str->storage, storage + pos, length * CHAR_SIZE);

    sub_str->storage[length] = 0;
    sub_str->len = length;
    return sub_str;
  }

  u32 find(char16_t ch, u32 pos = 0) const {
    if (pos >= len) return npos;
    auto res = char16_traits::find(storage + pos, len - pos, ch);
    return res ? res - storage : npos;
  }

  u32 find(const char16_t* str, u32 length, u32 pos = 0) const {
    if (!str || pos >= len || length > len - pos) [[unlikely]] {
      return npos;
    }
    if (length == 0) [[unlikely]] {
      return pos;
    }

    const char16_t* data_start = storage + pos;
    const char16_t* data_end = storage + len;
    const std::boyer_moore_searcher searcher(str, str + length);
    const auto it = std::search(data_start, data_end, searcher);

    return it != data_end ? it - storage : npos;
  }

  u32 rfind(const char16_t* str, u32 length, u32 pos = 0) const {
    if (!str) [[unlikely]] return npos;

    pos = std::min(pos, len);
    if (length == 0) [[unlikely]] return pos;

    const char16_t* data_start = storage;
    const char16_t* data_end = storage + std::min(pos + length, len);
    const auto it = std::find_end(data_start, data_end, str, str + length);

    return it != data_end ? it - storage : npos;
  }

  PrimitiveString* replace(GCHeap& heap, u32 pos, u32 length, u16string_view replacement) {
    if (pos > len) return nullptr;

    if (pos + length > len) {
      length = len - pos;
    }

    u32 rep_length = replacement.size();
    u32 new_length = len - length + rep_length;
    PrimitiveString* new_str = heap.new_prim_string(new_length);

    // Copy the part before the replaced segment
    if (pos > 0) {
      std::memcpy(new_str->storage, storage, pos * CHAR_SIZE);
    }
    // Copy the replacement segment
    std::memcpy(new_str->storage + pos, replacement.data(), rep_length * CHAR_SIZE);
    // Copy the part after the replaced segment
    if (pos + length < len) {
      std::memcpy(new_str->storage + pos + rep_length, storage + pos + length,
                  (len - pos - length) * CHAR_SIZE);
    }

    new_str->storage[new_length] = 0;
    new_str->len = new_length;

    return new_str;
  }

  char16_t char_at(size_t index) {
    return storage[index];
  }

  char16_t operator[](size_t index) const {
    return storage[index];
  }

  bool operator==(const PrimitiveString& other) const {
    return (len == other.len) && (std::memcmp(storage, other.storage, len * CHAR_SIZE) == 0);
  }

  bool operator!=(const PrimitiveString& other) const {
    return !(*this == other);
  }

  bool operator<(const PrimitiveString& other) const {
    return this->view() < other.view();
  }

  bool operator>(const PrimitiveString& other) const {
    return other < *this;
  }

  bool operator<=(const PrimitiveString& other) const {
    return !(other < *this);
  }

  bool operator>=(const PrimitiveString& other) const {
    return !(*this < other);
  }

  // u32 length() const {
  //   return len;
  // }

  // const char16_t* data() const {
  //   return storage;
  // }

  // char16_t* data() {
  //   return storage;
  // }

  bool empty() {
    return len == 0;
  }

 private:
  explicit PrimitiveString(u32 capacity) : cap(capacity) {
    alloc_count += 1;
  }

  void init(u16string_view str) {
    assert(str.size() < UINT32_MAX);
    len = str.size();
    std::memcpy(storage, str.data(), len * CHAR_SIZE);
    storage[len] = 0;
  }

  void init(const char16_t *str, size_t length) {
    assert(length < UINT32_MAX);
    len = length;
    std::memcpy(storage, str, len * CHAR_SIZE);
    storage[len] = 0;
  }

  u32 len;
  u32 cap;
  char16_t storage[0];
};

} // namespace njs

#endif //NJS_PRIMITIVE_STRING_H
