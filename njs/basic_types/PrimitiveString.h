#ifndef NJS_PRIMITIVE_STRING_H
#define NJS_PRIMITIVE_STRING_H

#include <string>
#include "njs/vm/NjsVM.h"
#include "njs/gc/GCObject.h"
#include "njs/gc/GCHeap.h"
#include "njs/common/conversion_helper.h"
#include "njs/common/common_def.h"
#include "njs/common/AtomPool.h"

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
    return "PrimitiveString";
  }

  u16string_view view(AtomPool& pool) const {
    if (not is_atom) {
      return {storage, (size_t)len};
    } else {
      return pool.get_string(atom);
    }
  }

  bool is_atom_string() {
    return atom;
  }

  u32 get_atom() {
    return atom;
  }

  u16string to_std_u16string(AtomPool& pool) const {
    return u16string(view(pool));
  }

  string to_std_string(AtomPool& pool) const {
    return to_u8string(view(pool));
  }

  PrimitiveString* concat(GCHeap& heap, PrimitiveString *str) {
    u16string_view str_view = str->view(heap.vm.atom_pool);
    return concat(heap, str_view.data(), str_view.length());
  }

  PrimitiveString* concat(GCHeap& heap, const char16_t* str, u32 length) {
    u16string_view this_str = view(heap.vm.atom_pool);
    u32 this_len = this_str.length();
    u32 new_length = this_len + length;
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
    std::memcpy(new_str->storage, this_str.data(), this_len * CHAR_SIZE);
    std::memcpy(new_str->storage + this_len, str, length * CHAR_SIZE);

    new_str->storage[new_length] = 0;
    new_str->len = new_length;
    return new_str;
  }

  PrimitiveString* substr(GCHeap& heap, u32 pos, u32 length = npos) const {
    u16string_view this_str = view(heap.vm.atom_pool);
    u32 this_len = this_str.length();

    pos = std::min(pos, this_len);
    length = std::min(length, this_len - pos);

    PrimitiveString *sub_str = heap.new_prim_string(length);
    std::memcpy(sub_str->storage, this_str.data() + pos, length * CHAR_SIZE);

    sub_str->storage[length] = 0;
    sub_str->len = length;
    return sub_str;
  }

  u32 find(AtomPool& pool, char16_t ch, u32 pos = 0) const {
    u16string_view this_str = view(pool);
    u32 this_len = this_str.length();

    if (pos >= this_len) return npos;
    auto res = char16_traits::find(this_str.data() + pos, this_len - pos, ch);
    return res ? res - this_str.data() : npos;
  }

  u32 find(AtomPool& pool, const char16_t* str, u32 length, u32 pos = 0) const {
    u16string_view this_str = view(pool);
    u32 this_len = this_str.length();

    if (!str || pos >= this_len || length > this_len - pos) [[unlikely]] {
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

  u32 rfind(AtomPool& pool, const char16_t* str, u32 length, u32 pos = 0) const {
    if (!str) [[unlikely]] return npos;

    u16string_view this_str = view(pool);
    u32 this_len = this_str.length();

    pos = std::min(pos, this_len);
    if (length == 0) [[unlikely]] return pos;

    const char16_t* data_start = this_str.data();
    const char16_t* data_end = data_start + std::min(pos + length, this_len);
    const auto it = std::find_end(data_start, data_end, str, str + length);

    return it != data_end ? it - data_start : npos;
  }

  PrimitiveString* replace(GCHeap& heap, u32 pos, u32 length, u16string_view replacement) {
    u16string_view this_str = view(heap.vm.atom_pool);
    u32 this_len = this_str.length();

    if (pos > this_len) return nullptr;

    if (pos + length > this_len) {
      length = this_len - pos;
    }

    u32 rep_length = replacement.size();
    u32 new_length = this_len - length + rep_length;
    PrimitiveString* new_str = heap.new_prim_string(new_length);

    // Copy the part before the replaced segment
    if (pos > 0) {
      std::memcpy(new_str->storage, this_str.data(), pos * CHAR_SIZE);
    }
    // Copy the replacement segment
    std::memcpy(new_str->storage + pos, replacement.data(), rep_length * CHAR_SIZE);
    // Copy the part after the replaced segment
    if (pos + length < this_len) {
      std::memcpy(new_str->storage + pos + rep_length, this_str.data() + pos + length,
                  (this_len - pos - length) * CHAR_SIZE);
    }

    new_str->storage[new_length] = 0;
    new_str->len = new_length;

    return new_str;
  }

  char16_t char_at(AtomPool& pool, size_t index) {
    u16string_view this_str = view(pool);
    return this_str[index];
  }

  bool equals(AtomPool& pool, PrimitiveString *other) {
    return view(pool) == other->view(pool);
  }

  bool ne(AtomPool& pool, PrimitiveString *other) {
    return view(pool) != other->view(pool);
  }

  bool lt(AtomPool& pool, PrimitiveString *other) const {
    return view(pool) < other->view(pool);
  }

  bool le(AtomPool& pool, PrimitiveString *other) const {
    return view(pool) <= other->view(pool);
  }

  bool ge(AtomPool& pool, PrimitiveString *other) const {
    return view(pool) >= other->view(pool);
  }

  bool gt(AtomPool& pool, PrimitiveString *other) const {
    return view(pool) > other->view(pool);
  }

//  char16_t operator[](size_t index) const {
//    return storage[index];
//  }

//  bool operator==(const PrimitiveString& other) const {
//    return (len == other.len) && (std::memcmp(storage, other.storage, len * CHAR_SIZE) == 0);
//  }

//  bool operator!=(const PrimitiveString& other) const {
//    return !(*this == other);
//  }

//  bool operator<(const PrimitiveString& other) const {
//    return this->view() < other.view();
//  }
//
//  bool operator>(const PrimitiveString& other) const {
//    return other < *this;
//  }
//
//  bool operator<=(const PrimitiveString& other) const {
//    return !(other < *this);
//  }
//
//  bool operator>=(const PrimitiveString& other) const {
//    return !(*this < other);
//  }

  // u32 length() const {
  //   return len;
  // }

  // const char16_t* data() const {
  //   return storage;
  // }

  // char16_t* data() {
  //   return storage;
  // }

  bool empty(AtomPool& pool) {
    return view(pool).empty();
  }

 private:
  explicit PrimitiveString(u32 capacity) : cap(capacity) {
    alloc_count += 1;
  }
  ~PrimitiveString() override = default;

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

  bool is_atom {false};
  u32 atom;
  u32 len;
  u32 cap;
  char16_t storage[0];
};

} // namespace njs

#endif //NJS_PRIMITIVE_STRING_H
