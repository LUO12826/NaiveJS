#ifndef NJS_STRING_POOL_H
#define NJS_STRING_POOL_H

#include <string>
#include <cassert>
#include <unordered_map>
#include "njs/include/robin_hood.h"
#include "njs/include/SmallVector.h"
#include "njs/basic_types/String.h"

namespace njs {

using llvm::SmallVector;
using robin_hood::unordered_map;
using u32 = uint32_t;
using std::u16string;
using std::vector;
using std::u16string_view;
using std::optional;

class StringPool {
 public:
  StringPool() {
    ATOM_length = atomize(u"length");              // 0
    ATOM_prototype = atomize(u"prototype");        // 1
    ATOM_charAt = atomize(u"charAt");              // 2
    ATOM_constructor = atomize(u"constructor");    // 3
    ATOM___proto__ = atomize(u"__proto__");        // 4
    ATOM_toString = atomize(u"toString");          // 5
    ATOM_valueOf = atomize(u"valueOf");            // 6
    ATOM_toPrimitive = atomize(u"toPrimitive");    // 7
    ATOM_iterator = atomize(u"iterator");          // 8
  }

  u32 atomize(u16string_view str_view);
  u32 atomize_u32(u32 num);
  u32 atomize_symbol();
  u32 atomize_symbol_desc(u16string_view str_view);
  u16string_view get_string(size_t index);
  optional<u16string_view> get_symbol_desc(size_t index);
  bool has_string(u16string_view str_view);
  void record_static_atom_count();

  inline static u32 ATOM_length;
  inline static u32 ATOM_prototype;
  inline static u32 ATOM_charAt;
  inline static u32 ATOM_constructor;
  inline static u32 ATOM___proto__;
  inline static u32 ATOM_toString;
  inline static u32 ATOM_valueOf;
  inline static u32 ATOM_toPrimitive;
  inline static u32 ATOM_iterator;

 private:
  struct Slot {
    bool is_symbol;
    bool symbol_has_desc;
    bool gc_mark {false};
    String str;

    Slot(): is_symbol(true) {}
    Slot(const u16string& str): is_symbol(false), str(str) {}
    Slot(u16string_view str_view): is_symbol(false), str(str_view) {}
  };

  u32 next_id {0};
  u32 static_atom_count {0};
  unordered_map<u16string_view, u32> pool;
  vector<Slot> string_list;
};

inline u32 StringPool::atomize(u16string_view str_view) {
  if (pool.contains(str_view)) {
    return pool[str_view];
  }
  else {
    // copy this string and put it in the list.
    string_list.emplace_back(str_view);
    // now the string_view in the pool is viewing the string in the list.
    pool.emplace(string_list.back().str.view(), next_id);
    next_id += 1;
    // TODO: in this case, throw an error (although this is not likely to happen)
    assert(next_id <= (UINT32_MAX >> 1));
    return next_id - 1;
  }
}

inline u32 StringPool::atomize_u32(njs::u32 num) {
  if (likely(num < (1u << 31))) {
    return (1 << 31) | num;
  } else {
    auto str = to_u16string(std::to_string(num));
    return atomize(str);
  }
}

inline u32 StringPool::atomize_symbol() {
  string_list.emplace_back();
  next_id += 1;
  return next_id - 1;
}

inline u32 StringPool::atomize_symbol_desc(u16string_view str_view) {
  auto& new_slot = string_list.emplace_back();
  new_slot.is_symbol = true;
  new_slot.symbol_has_desc = true;
  new_slot.str = String(str_view);
  next_id += 1;
  return next_id - 1;
}

inline u16string_view StringPool::get_string(size_t index) {
  assert(!string_list[index].is_symbol);
  return string_list[index].str.view();
}

inline optional<u16string_view> StringPool::get_symbol_desc(size_t index) {
  auto& slot = string_list[index];
  assert(slot.is_symbol);
  if (slot.symbol_has_desc) {
    return slot.str.view();
  } else {
    return std::nullopt;
  }
}

inline bool StringPool::has_string(u16string_view str_view) {
  return pool.contains(str_view);
}

inline void StringPool::record_static_atom_count() {
  static_atom_count = string_list.size();
}

} // namespace njs

#endif // NJS_STRING_POOL_H
