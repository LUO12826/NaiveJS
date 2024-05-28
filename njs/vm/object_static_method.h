#ifndef NJS_OBJECT_STATIC_METHOD_H
#define NJS_OBJECT_STATIC_METHOD_H

#include "Completion.h"
#include "njs/basic_types/JSObject.h"
#include "njs/basic_types/JSObjectPrototype.h"
#include "njs/basic_types/conversion.h"
#include "njs/common/ArrayRef.h"
#include "njs/common/common_def.h"

namespace njs {

inline Completion Object_defineProperty(vm_func_This_args_flags) {
  assert(args.size() >= 3);
  if (not args[0].is_object()) {
    return Completion::with_throw(vm.build_error_internal(
        JS_TYPE_ERROR, u"Object.defineProperty called on non-object"));
  }
  if (not args[2].is_object()) {
    return Completion::with_throw(vm.build_error_internal(
        JS_TYPE_ERROR, u"Property description must be an object"));
  }

  JSPropDesc desc = TRY_ERR_COMP(args[2].as_object()->to_property_descriptor(vm));
  JSValue key = TRY_COMP_COMP(js_to_property_key(vm, args[1]));
  bool succeeded = TRY_ERR_COMP(args[0].as_object()->define_own_property(key, desc));
  return JSValue(succeeded);
}

inline Completion Object_hasOwn(vm_func_This_args_flags) {
  assert(args.size() >= 2);
  if (not args[0].is_object()) {
    return Completion::with_throw(vm.build_error_internal(
        JS_TYPE_ERROR, u"Object.hasOwn called on non-object"));
  }
  return JSObjectPrototype::hasOwnProperty(vm, func, args[0], args.subarray(1, 1), flags);
}

}

#endif // NJS_OBJECT_STATIC_METHOD_H
