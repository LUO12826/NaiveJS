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
  static inline uint64_t append_count {0};
  static inline uint64_t fast_concat_count {0};
  static inline uint64_t fast_append_count {0};
  static inline uint64_t alloc_count {0};
  static inline uint64_t concat_length {0};

  static constexpr u32 npos = UINT32_MAX;

  PrimitiveString(const PrimitiveString& other) = delete;
  PrimitiveString(PrimitiveString&& other) = delete;
  PrimitiveString& operator=(PrimitiveString&& other) = delete;
  PrimitiveString& operator=(const PrimitiveString& other) = delete;

  std::string description() override {
    return "PrimitiveString(" + to_std_string() + ")";
  }

  u16string_view view() const {
    if (str_ref != nullptr) {
      return {str_ref, (size_t)len};
    } else {
      return {storage, (size_t)len};
    }
  }

  u16string to_std_u16string() const {
    return u16string(view());
  }

  string to_std_string() const {
    return to_u8string(view());
  }

  PrimitiveString* concat(GCHeap& heap, PrimitiveString *str) {
    return concat(heap, str->data(), str->length());
  }

  PrimitiveString* concat(GCHeap& heap, const char16_t* str, u32 length) {
    u32 new_length = len + length;
    assert(new_length < UINT32_MAX);

    concat_count += 1;
    concat_length += new_length;

    // This optimization doesn't look very effective
    PrimitiveString *new_str;
    if (get_ref_count() == 0 && new_length < cap) {
      fast_concat_count += 1;
      new_str = this;
    } else {
      new_str = heap.new_prim_string(new_length);
      std::memcpy(new_str->storage, view().data(), len * CHAR_SIZE);
    }

//    PrimitiveString *new_str = heap.new_prim_string(new_length);
//    std::memcpy(new_str->storage, this_str.data(), len * CHAR_SIZE);
    std::memcpy(new_str->storage + len, str, length * CHAR_SIZE);

    new_str->storage[new_length] = 0;
    new_str->len = new_length;
    return new_str;
  }

  PrimitiveString* append(GCHeap& heap, PrimitiveString *str) {
    return append(heap, str->data(), str->length());
  }

  PrimitiveString* append(GCHeap& heap, const char16_t* str, u32 length) {
    u32 new_length = len + length;
    assert(new_length < UINT32_MAX);
    assert(get_ref_count() <= 1);
    append_count += 1;

    PrimitiveString *new_str;
    if (new_length < cap) {
      fast_append_count += 1;
      new_str = this;
    } else {
      new_str = heap.new_prim_string(new_length);
      std::memcpy(new_str->storage, view().data(), len * CHAR_SIZE);
    }

    std::memcpy(new_str->storage + len, str, length * CHAR_SIZE);

    new_str->storage[new_length] = 0;
    new_str->len = new_length;
    return new_str;
  }

  PrimitiveString* substr(GCHeap& heap, u32 pos, u32 length = npos) const {
    u16string_view this_str = view();

    pos = std::min(pos, len);
    length = std::min(length, len - pos);

    PrimitiveString *sub_str = heap.new_prim_string(length);
    std::memcpy(sub_str->storage, this_str.data() + pos, length * CHAR_SIZE);

    sub_str->storage[length] = 0;
    sub_str->len = length;
    return sub_str;
  }

  u32 find(char16_t ch, u32 pos = 0) const {
    u16string_view this_str = view();

    if (pos >= len) return npos;
    auto res = char16_traits::find(this_str.data() + pos, len - pos, ch);
    return res ? res - this_str.data() : npos;
  }

  u32 find(const char16_t* str, u32 length, u32 pos = 0) const {
    u16string_view this_str = view();

    if (!str || pos >= len || length > len - pos) [[unlikely]] {
      return npos;
    }
    if (length == 0) [[unlikely]] {
      return pos;
    }

    const char16_t* data_start = this_str.data() + pos;
    const char16_t* data_end = this_str.end();
    const std::boyer_moore_searcher searcher(str, str + length);
    const auto it = std::search(data_start, data_end, searcher);

    return it != data_end ? it - this_str.data() : npos;
  }

  u32 rfind(const char16_t* str, u32 length, u32 pos = 0) const {
    if (!str) [[unlikely]] return npos;

    u16string_view this_str = view();

    pos = std::min(pos, len);
    if (length == 0) [[unlikely]] return pos;

    const char16_t* data_start = this_str.data();
    const char16_t* data_end = data_start + std::min(pos + length, len);
    const auto it = std::find_end(data_start, data_end, str, str + length);

    return it != data_end ? it - data_start : npos;
  }

  PrimitiveString* replace(GCHeap& heap, u32 pos, u32 length, u16string_view replacement) {
    if (pos > len) return nullptr;

    if (pos + length > len) {
      length = len - pos;
    }
    u16string_view this_str = view();

    u32 rep_length = replacement.size();
    u32 new_length = len - length + rep_length;
    PrimitiveString* new_str = heap.new_prim_string(new_length);

    // Copy the part before the replaced segment
    if (pos > 0) {
      std::memcpy(new_str->storage, this_str.data(), pos * CHAR_SIZE);
    }
    // Copy the replacement segment
    std::memcpy(new_str->storage + pos, replacement.data(), rep_length * CHAR_SIZE);
    // Copy the part after the replaced segment
    if (pos + length < len) {
      std::memcpy(new_str->storage + pos + rep_length, this_str.data() + pos + length,
                  (len - pos - length) * CHAR_SIZE);
    }

    new_str->storage[new_length] = 0;
    new_str->len = new_length;

    return new_str;
  }

  char16_t operator[](size_t index) const {
    return view()[index];
  }

  // after c++20: can use operator <=> here
  bool operator==(const PrimitiveString& other) const {
    return view() == other.view();
  }

  bool operator!=(const PrimitiveString& other) const {
    return view() != other.view();
  }

  bool operator<(const PrimitiveString& other) const {
    return view() < other.view();
  }

  bool operator>(const PrimitiveString& other) const {
    return view() > other.view();
  }

  bool operator<=(const PrimitiveString& other) const {
    return view() <= other.view();
  }

  bool operator>=(const PrimitiveString& other) const {
    return view() >= other.view();
  }

   u32 length() const {
     return len;
   }

   const char16_t* data() const {
     return str_ref == nullptr ? storage : str_ref;
   }

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

  void init_with_ref(const char16_t *str, size_t length) {
    assert(length < UINT32_MAX);
    len = length;
    str_ref = str;
  }

  u32 len;
  u32 cap;
  const char16_t *str_ref {nullptr};
  char16_t storage[0];
};

} // namespace njs

#endif //NJS_PRIMITIVE_STRING_H
