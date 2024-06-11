#ifndef NJS_ATOM_POOL_H
#define NJS_ATOM_POOL_H

#include <cassert>
#include <string>
#include <unordered_map>
#include "njs/basic_types/String.h"
#include "njs/basic_types/atom.h"
#include "njs/include/robin_hood.h"
#include "njs/include/MurmurHash3.h"
#include "njs/parser/lexing_helper.h"
#include "njs/common/conversion_helper.h"

namespace njs {

using robin_hood::unordered_flat_map;
using u32 = uint32_t;
using std::u16string;
using std::vector;
using std::u16string_view;
using std::optional;

struct AtomStats {
  size_t atomize_str_count {0};
  size_t static_atomize_str_count {0};
};

class AtomPool {
 public:
  AtomPool() {
    k_ = atomize(u"");
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
    k_match = atomize(u"match");
    k_matchAll = atomize(u"matchAll");
    k_replace = atomize(u"replace");
    k_search = atomize(u"search");
    k_split = atomize(u"split");
    k_sym_iterator = atomize_symbol_desc(u"iterator");
    k_sym_match = atomize_symbol_desc(u"match");
    k_sym_matchAll = atomize_symbol_desc(u"matchAll");
    k_sym_replace = atomize_symbol_desc(u"replace");
    k_sym_search = atomize_symbol_desc(u"search");
    k_sym_split = atomize_symbol_desc(u"split");
    k_next = atomize(u"next");
    k_done = atomize(u"done");
    k_value = atomize(u"value");
    k_enumerable = atomize(u"enumerable");
    k_configurable = atomize(u"configurable");
    k_writable = atomize(u"writable");
    k_get = atomize(u"get");
    k_set = atomize(u"set");
    k_lastIndex = atomize(u"lastIndex");
    k_name = atomize(u"name");
  }

  AtomPool(const AtomPool& other) = delete;
  AtomPool(AtomPool&& other);
  ~AtomPool();

  u32 atomize(u16string_view str_view);
  u32 atomize_no_uint(u16string_view str_view);
  u32 atomize_u32(u32 num);
  u32 atomize_symbol();
  u32 atomize_symbol_desc(u16string_view desc);
  u16string_view get_string(u32 atom);
  optional<u16string_view> get_symbol_desc(u32 symbol);
  bool has_string(u16string_view str_view);
  void record_static_atom_count();

  inline static u32 k_;
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
  // atoms above are also in VM's string_const pool.
  inline static u32 k_length;
  inline static u32 k_prototype;
  inline static u32 k_charAt;
  inline static u32 k_constructor;
  inline static u32 k___proto__;
  inline static u32 k_toString;
  inline static u32 k_valueOf;
  inline static u32 k_toPrimitive;

  inline static u32 k_iterator;
  inline static u32 k_match;
  inline static u32 k_matchAll;
  inline static u32 k_replace;
  inline static u32 k_search;
  inline static u32 k_split;

  inline static u32 k_sym_iterator;
  inline static u32 k_sym_match;
  inline static u32 k_sym_matchAll;
  inline static u32 k_sym_replace;
  inline static u32 k_sym_search;
  inline static u32 k_sym_split;
  
  inline static u32 k_next;
  inline static u32 k_done;
  inline static u32 k_value;
  inline static u32 k_enumerable;
  inline static u32 k_configurable;
  inline static u32 k_writable;
  inline static u32 k_get;
  inline static u32 k_set;
  inline static u32 k_lastIndex;
  inline static u32 k_name;

  AtomStats stats;
 private:
  struct Slot {
    bool is_symbol;
    bool symbol_has_desc;
    bool gc_mark {false};
    struct {
      char16_t *data {nullptr};
      size_t len {0};
    } str;

    Slot() = default;

    explicit Slot(const u16string& str): is_symbol(false) {
      init_str(str.data(), str.size());
    }

    explicit Slot(u16string_view str_view): is_symbol(false) {
      init_str(str_view.data(), str_view.size());
    }

    void init_str(const char16_t *data, size_t length) {
      str.len = length;
      str.data = new char16_t[length + 1];
      memcpy(str.data, data, length * sizeof(char16_t));
      str.data[length] = 0;
    }

    void dispose() {
      delete[] str.data;
      // must set to null to avoid double free (since we didn't implement
      // the copy constructor or the move constructor)
      str.data = nullptr;
    }

    u16string_view str_view() {
      return {str.data, str.len};
    }
  };

  struct KeyHasher {
    std::size_t operator()(const u16string_view& sv) const {
      uint64_t output[2];
      MurmurHash3_x64_128(sv.data(), sv.size() * sizeof(char16_t), 31, &output);
      return output[0] ^ output[1];
    }
  };

  u32 next_id {0};
  u32 static_atom_count {0};
  unordered_flat_map<u16string_view, u32, KeyHasher> pool;
  vector<Slot> string_list;
};

inline AtomPool::AtomPool(AtomPool&& other)
    : next_id(other.next_id),
      static_atom_count(other.static_atom_count),
      pool(std::move(other.pool)),
      string_list(std::move(other.string_list))
{
  other.string_list.clear();
}

inline AtomPool::~AtomPool() {
  for (auto& slot : string_list) {
    slot.dispose();
  }
}

inline u32 AtomPool::atomize(u16string_view str_view) {
  stats.atomize_str_count += 1;
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
      pool.emplace(string_list.back().str_view(), next_id);
      next_id += 1;
      // TODO: in this case, throw an error (although this is not likely to happen)
      assert(next_id <= ATOM_STR_SYM_MAX);
      return next_id - 1;
    }
  }
}

inline u32 AtomPool::atomize_no_uint(std::u16string_view str_view) {
  stats.atomize_str_count += 1;
  if (pool.contains(str_view)) {
    return pool[str_view];
  }
  else {
    string_list.emplace_back(str_view);
    pool.emplace(string_list.back().str_view(), next_id);
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
  auto& new_slot = string_list.emplace_back();
  new_slot.is_symbol = true;
  new_slot.symbol_has_desc = false;
  next_id += 1;
  return next_id - 1;
}

inline u32 AtomPool::atomize_symbol_desc(u16string_view desc) {
  auto& new_slot = string_list.emplace_back();
  new_slot.is_symbol = true;
  new_slot.symbol_has_desc = true;
  new_slot.init_str(desc.data(), desc.size());
  next_id += 1;
  return next_id - 1;
}

inline u16string_view AtomPool::get_string(u32 atom) {
  assert(atom_is_str_sym(atom));
  assert(!string_list[atom].is_symbol);
  return string_list[atom].str_view();
}

inline optional<u16string_view> AtomPool::get_symbol_desc(u32 symbol) {
  assert(atom_is_str_sym(symbol));
  auto& slot = string_list[symbol];
  assert(slot.is_symbol);
  if (slot.symbol_has_desc) {
    return slot.str_view();
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
  stats.static_atomize_str_count = stats.atomize_str_count;
}

} // namespace njs

#endif // NJS_ATOM_POOL_H
