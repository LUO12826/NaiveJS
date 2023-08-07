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
    ATOM_length = add_string(u"length");     // 0
    ATOM_prototype = add_string(u"prototype");  // 1
  }

  u32 add_string(u16string_view str_view);
  u16string& get_string(size_t index);
  std::vector<u16string>& get_string_list();

  inline static u32 ATOM_length;
  inline static u32 ATOM_prototype;

 private:
  u32 next_id {0};
  unordered_map<u16string, u32> pool;
  std::vector<u16string> string_list;
};

inline u32 StringPool::add_string(u16string_view str_view) {
  u16string str(str_view);
  if (pool.contains(str)) {
    return pool[str];
  }
  else {
    // copy this string and put it in the list.
    string_list.emplace_back(str);
    // now the string_view in the pool is viewing the string in the list.
    pool.emplace(std::move(str), next_id);
    next_id += 1;
    return next_id - 1;
  }
}

inline std::vector<u16string>& StringPool::get_string_list() {
  return string_list;
}

inline u16string& StringPool::get_string(size_t index) {
  return string_list[index];
}

} // namespace njs

#endif // NJS_STRING_POOL_H
