#include "StringPool.h"
#include <optional>

namespace njs {

u32 StringPool::add_string(u16string str) {
  u32 id = next_id;
  pool[std::move(str)] = id;
  next_id += 1;
  return id;
}

optional<u32> StringPool::get_string_id(const u16string& str) {
  if (next_id == 0) return std::nullopt;

  u32 test_id = pool[str];
  return test_id != 0;
}

} // namespace njs