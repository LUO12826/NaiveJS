#ifndef NJS_ATOM_POOL_H
#define NJS_ATOM_POOL_H

#include "njs/basic_types/String.h"
#include "njs/basic_types/atom.h"
#include "njs/include/robin_hood.h"
#include "njs/parser/lexing_helper.h"
#include <cassert>
#include <string>
#include <unordered_map>

namespace njs {

using robin_hood::unordered_map;
using u32 = uint32_t;
using std::u16string;
using std::vector;
using std::u16string_view;
using std::optional;

class AtomPool {
 public:
  AtomPool() {
    k_undefined = atomize(u"undefined");
    k_null = atomize(u"null");
    k_true = atomize(u"true");
    k_false = atomize(u"false");
    k_number = atomize(u"number");
    k_boolean = atomize(u"boolean");
    k_string = atomize(u"string");
    k_object = atomize(u"object");
    k_symbol = atomize(u"symbol");
    k_function = atomize(u"function");
    k_length = atomize(u"length");
    k_prototype = atomize(u"prototype");
    k_charAt = atomize(u"charAt");
    k_constructor = atomize(u"constructor");
    k___proto__ = atomize(u"__proto__");
    k_toString = atomize(u"toString");
    k_valueOf = atomize(u"valueOf");
    k_toPrimitive = atomize(u"toPrimitive");
    k_iterator = atomize(u"iterator");
    k_sym_iterator = atomize_symbol_desc(u"iterator");
    k_next = atomize(u"next");
    k_done = atomize(u"done");
    k_value = atomize(u"value");
    k_enumerable = atomize(u"enumerable");
    k_configurable = atomize(u"configurable");
    k_writable = atomize(u"writable");
    k_get = atomize(u"get");
    k_set = atomize(u"set");
  }

  u32 atomize(u16string_view str_view);
  u32 atomize_no_uint(u16string_view str_view);
  u32 atomize_u32(u32 num);
  u32 atomize_symbol();
  u32 atomize_symbol_desc(u16string_view str_view);
  u16string_view get_string(u32 atom);
  optional<u16string_view> get_symbol_desc(u32 symbol);
  bool has_string(u16string_view str_view);
  void record_static_atom_count();

  inline static u32 k_undefined;
  inline static u32 k_null;
  inline static u32 k_true;
  inline static u32 k_false;
  inline static u32 k_number;
  inline static u32 k_boolean;
  inline static u32 k_string;
  inline static u32 k_object;
  inline static u32 k_symbol;
  inline static u32 k_function;
  inline static u32 k_length;
  inline static u32 k_prototype;
  inline static u32 k_charAt;
  inline static u32 k_constructor;
  inline static u32 k___proto__;
  inline static u32 k_toString;
  inline static u32 k_valueOf;
  inline static u32 k_toPrimitive;
  inline static u32 k_iterator;
  inline static u32 k_sym_iterator;
  inline static u32 k_next;
  inline static u32 k_done;
  inline static u32 k_value;
  inline static u32 k_enumerable;
  inline static u32 k_configurable;
  inline static u32 k_writable;
  inline static u32 k_get;
  inline static u32 k_set;

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

inline u32 AtomPool::atomize(u16string_view str_view) {
  if (pool.contains(str_view)) {
    return pool[str_view];
  }
  else {
    uint64_t idx = scan_index_literal(str_view);
    if (idx != -1 && idx <= ATOM_INT_MAX) [[unlikely]] {
      return ATOM_INT_TAG | (u32)idx;
    } else {
      // copy this string and put it in the list.
      string_list.emplace_back(str_view);
      // now the string_view in the pool is viewing the string in the list.
      pool.emplace(string_list.back().str.view(), next_id);
      next_id += 1;
      // TODO: in this case, throw an error (although this is not likely to happen)
      assert(next_id <= ATOM_STR_SYM_MAX);
      return next_id - 1;
    }
  }
}

inline u32 AtomPool::atomize_no_uint(std::u16string_view str_view) {
  if (pool.contains(str_view)) {
    return pool[str_view];
  }
  else {
    string_list.emplace_back(str_view);
    pool.emplace(string_list.back().str.view(), next_id);
    next_id += 1;
    assert(next_id <= ATOM_STR_SYM_MAX);
    return next_id - 1;
  }
}

inline u32 AtomPool::atomize_u32(njs::u32 num) {
  if (num <= ATOM_INT_MAX) [[likely]] {
    return ATOM_INT_TAG | num;
  } else {
    auto str = to_u16string(num);
    return atomize(str);
  }
}

inline u32 AtomPool::atomize_symbol() {
  string_list.emplace_back();
  next_id += 1;
  return next_id - 1;
}

inline u32 AtomPool::atomize_symbol_desc(u16string_view desc) {
  auto& new_slot = string_list.emplace_back();
  new_slot.is_symbol = true;
  new_slot.symbol_has_desc = true;
  new_slot.str = String(desc);
  next_id += 1;
  return next_id - 1;
}

inline u16string_view AtomPool::get_string(u32 atom) {
  assert(atom_is_str_sym(atom));
  assert(!string_list[atom].is_symbol);
  return string_list[atom].str.view();
}

inline optional<u16string_view> AtomPool::get_symbol_desc(u32 symbol) {
  assert(atom_is_str_sym(symbol));
  auto& slot = string_list[symbol];
  assert(slot.is_symbol);
  if (slot.symbol_has_desc) {
    return slot.str.view();
  } else {
    return std::nullopt;
  }
}

inline bool AtomPool::has_string(u16string_view str_view) {
  if (pool.contains(str_view)) {
    return true;
  } else if (auto idx = scan_index_literal(str_view);
              idx != -1 && idx <= ATOM_INT_MAX) {
    return true;
  } else {
    return false;
  }
}

inline void AtomPool::record_static_atom_count() {
  static_atom_count = string_list.size();
}

} // namespace njs

#endif // NJS_ATOM_POOL_H
