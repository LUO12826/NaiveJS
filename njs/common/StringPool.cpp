#include "StringPool.h"
#include <optional>

namespace njs {

u32 StringPool::add_string(u16string str) {
  u32 id = next_id;
  pool[std::move(str)] = id;
  next_id += 1;
  return id;
}

u32 StringPool::add_string(u16string_view str_view) {
  u32 id = next_id;
  pool[u16string(str_view)] = id;
  next_id += 1;
  return id;
}

optional<u32> StringPool::get_string_id(const u16string& str) {
  if (next_id == 0) return std::nullopt;

  u32 test_id = pool[str];
  return test_id != 0;
}

SmallVector<u16string, 10> StringPool::to_list() {
  SmallVector<u16string, 10> str_list;
  str_list.resize(pool.size());

  for (auto& kv_pair : pool) {
    u32 idx = kv_pair.second;
    str_list[idx] = std::move(kv_pair.first);
  }
  return str_list;
}

} // namespace njs