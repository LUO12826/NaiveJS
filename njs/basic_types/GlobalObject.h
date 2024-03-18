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

  JSValue& get_or_add_prop(NjsVM& vm, const u16string& name) {

    int64_t name_atom = vm.str_pool.atomize(name);
    auto iter = props_index_map.find(name_atom);
    if (iter != props_index_map.end()) {
      return vm.stack_get_at_index(iter->second);
    }
    else {
      JSValue val_ref = JSObject::get_prop(name_atom, true);
      return *val_ref.val.as_JSValue;
    }
  }

  JSValue get_prop(NjsVM& vm, int64_t atom, bool get_ref) {
    auto iter = props_index_map.find(atom);
    if (iter != props_index_map.end()) {
      if (not get_ref) return vm.stack_get_at_index(iter->second);
      else return JSValue(&vm.stack_get_at_index(iter->second));
    }
    else {
      return JSObject::get_prop(atom, get_ref);
    }
  }

  JSValue get_prop(NjsVM& vm, const u16string& name, bool get_ref) {
    if (!get_ref && !vm.str_pool.has_string(name)) {
      return JSValue::undefined;
    }
    int64_t name_atom = vm.str_pool.atomize(name);
    return get_prop(vm, name_atom, get_ref);
  }

  unordered_map<int64_t, u32> props_index_map;
};

} // namespace njs

#endif // NJS_GLOBAL_OBJECT_H