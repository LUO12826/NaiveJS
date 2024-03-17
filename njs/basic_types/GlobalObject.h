#ifndef NJS_GLOBAL_OBJECT_H
#define NJS_GLOBAL_OBJECT_H

#include "njs/vm/NjsVM.h"
#include "JSObject.h"
#include "njs/include/robin_hood.h"
#include <cstdint>
#include <optional>

namespace njs {

using u32 = uint32_t;
using robin_hood::unordered_map;
using std::u16string;
using std::optional;

// In JavaScript, variables defined in the global space will actually become properties of the
// global object. However, when NaiveJS retrieves global variables, it will actually retrieve them
// as if they were indexed in the stack space as local variable lookup (i.e., the names of the
// variables are erased). In such a case, to support accessing the properties of the global object
// by name, use this class for property name-to-index mapping.

class GlobalObject: public JSObject {
 public:
  GlobalObject(): JSObject(ObjectClass::CLS_GLOBAL_OBJ) {}

  JSValue& get_or_add_prop(const u16string& name, NjsVM& vm) {

    int64_t name_atom = vm.str_pool.atomize(name);
    auto iter = props_index_map.find(name_atom);
    if (iter != props_index_map.end()) {
      return vm.stack_get_at_index(iter->second);
    } else {
      JSValue val_ref = JSObject::get_prop(name_atom, true);
      return *val_ref.val.as_JSValue;
    }
  }

  JSValue get_prop(int64_t atom, NjsVM& vm) {
    auto iter = props_index_map.find(atom);
    if (iter != props_index_map.end()) {
      return JSValue(&vm.stack_get_at_index(iter->second));
    } else {
      return JSObject::get_prop(atom, false);
    }
  }

  unordered_map<int64_t, u32> props_index_map;
};

} // namespace njs

#endif // NJS_GLOBAL_OBJECT_H