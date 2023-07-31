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
    add_string(u"length");     // 0
    add_string(u"prototype");  // 1
  }

  u32 add_string(u16string_view str_view);
  std::vector<u16string>& get_string_list();

 private:
  u32 next_id {0};
  unordered_map<u16string, u32> pool;
  std::vector<u16string> string_list;
};

} // namespace njs

#endif // NJS_STRING_POOL_H
