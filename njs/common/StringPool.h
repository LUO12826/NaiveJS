#ifndef NJS_STRING_POOL_H
#define NJS_STRING_POOL_H

#include <string>
#include <unordered_map>
#include "njs/include/robin_hood.h"
#include "njs/include/SmallVector.h"

namespace njs {

using llvm::SmallVector;
using robin_hood::unordered_map;
using u32 = uint32_t;
using std::u16string;
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

  u32 atomize_sv(u16string_view str_view);
  u32 atomize(const u16string& str);
  u16string& get_string(size_t index);
  bool has_string(u16string_view str_view);
  bool has_string(const u16string& str);
  std::vector<u16string>& get_string_list();
  void record_static_atom_count();

  inline static int64_t ATOM_length;
  inline static int64_t ATOM_prototype;
  inline static int64_t ATOM_charAt;
  inline static int64_t ATOM_constructor;
  inline static int64_t ATOM___proto__;
  inline static int64_t ATOM_toString;
  inline static int64_t ATOM_valueOf;
  inline static int64_t ATOM_toPrimitive;
  inline static int64_t ATOM_iterator;

 private:
  u32 next_id {0};
  u32 static_atom_count {0};
  unordered_map<u16string, u32> pool;
  std::vector<u16string> string_list;
};

inline u32 StringPool::atomize(const u16string& str) {
  if (pool.contains(str)) {
    return pool[str];
  }
  else {
    // copy this string and put it in the list.
    string_list.emplace_back(str);
    // now the string_view in the pool is viewing the string in the list.
    pool.emplace(str, next_id);
    next_id += 1;
    return next_id - 1;
  }
}

inline u32 StringPool::atomize_sv(u16string_view str_view) {
  return atomize(u16string(str_view));
}

inline u16string& StringPool::get_string(size_t index) {
  return string_list[index];
}

inline bool StringPool::has_string(u16string_view str_view) {
  return pool.contains(u16string(str_view));
}

inline bool StringPool::has_string(const u16string& str) {
  return pool.contains(str);
}


inline std::vector<u16string>& StringPool::get_string_list() {
  return string_list;
}

inline void StringPool::record_static_atom_count() {
  static_atom_count = pool.size();
}

} // namespace njs

#endif // NJS_STRING_POOL_H
