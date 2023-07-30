#include "StringPool.h"
#include <optional>

namespace njs {

u32 StringPool::add_string(u16string_view str_view) {
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

std::vector<u16string>& StringPool::get_string_list() {
  return string_list;
}

} // namespace njs