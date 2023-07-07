#ifndef NJS_STRING_POOL_H
#define NJS_STRING_POOL_H

#include <string>
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
  u32 add_string(u16string str);

  u32 add_string(u16string_view str_view);

  optional<u32> get_string_id(const u16string& str);

  SmallVector<u16string, 10> to_list();

 private:
  u32 next_id {0};
  unordered_map<u16string, u32> pool;
};

} // namespace njs

#endif // NJS_STRING_POOL_H
