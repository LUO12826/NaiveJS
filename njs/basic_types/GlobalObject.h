#ifndef NJS_GLOBAL_OBJECT_H
#define NJS_GLOBAL_OBJECT_H

#include "JSObject.h"
#include "njs/include/robin_hood.h"
#include <cstdint>

namespace njs {

using u32 = uint32_t;
using robin_hood::unordered_map;
using std::u16string;

// In JavaScript, variables defined in the global space will actually become properties of the
// global object. However, when NaiveJS retrieves global variables, it will actually retrieve them
// as if they were indexed in the stack space as local variable lookup (i.e., the names of the
// variables are erased). In such a case, to support accessing the properties of the global object
// by name, use this class for property name-to-index mapping.

class GlobalObject {
 public:
  GlobalObject(unordered_map<u16string, u32>& props_map)
      : static_props_cnt(props_map.size()), props_index_map(std::move(props_map)) {}

 private:
  u32 static_props_cnt;
  unordered_map<u16string, u32> props_index_map;
  JSObject dynamic_props;
};

} // namespace njs

#endif // NJS_GLOBAL_OBJECT_H