#ifndef NJS_STRING_POOL_H
#define NJS_STRING_POOL_H

#include <string>
#include "njs/include/robin_hood.h"

namespace njs {

using robin_hood::unordered_map;
using u32 = uint32_t;
using std::u16string;
using std::optional;

class StringPool {
 public:
  u32 add_string(u16string str);

  optional<u32> get_string_id(const u16string& str);

 private:
  u32 next_id {0};
  unordered_map<u16string, u32> pool;
};

} // namespace njs

#endif // NJS_STRING_POOL_H
