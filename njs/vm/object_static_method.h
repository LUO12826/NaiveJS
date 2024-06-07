#ifndef NJS_OBJECT_STATIC_METHOD_H
#define NJS_OBJECT_STATIC_METHOD_H

#include "Completion.h"
#include "njs/basic_types/JSObject.h"
#include "njs/basic_types/JSObjectPrototype.h"
#include "njs/basic_types/JSValue.h"
#include "njs/basic_types/conversion.h"
#include "njs/common/ArrayRef.h"
#include "njs/common/JSErrorType.h"
#include "njs/common/common_def.h"
#include "njs/utils/macros.h"
#include "njs/vm/ErrorOr.h"

namespace njs {

inline Completion Object_defineProperty(vm_func_This_args_flags) {
  assert(args.size() >= 3);
  if (not args[0].is_object()) {
    return CompThrow(vm.build_error_internal(
        JS_TYPE_ERROR, u"Object.defineProperty called on non-object"));
  }
  if (not args[2].is_object()) {
    return CompThrow(vm.build_error_internal(
        JS_TYPE_ERROR, u"Property description must be an object"));
  }

  JSPropDesc desc = TRY_COMP(args[2].as_object->to_property_descriptor(vm));
  JSValue key = TRYCC(js_to_property_key(vm, args[1]));
  bool succeeded = TRY_COMP(args[0].as_object->define_own_property(key, desc));
  return JSValue(succeeded);
}

inline Completion Object_hasOwn(vm_func_This_args_flags) {
  assert(args.size() >= 2);
  if (not args[0].is_object()) {
    return CompThrow(vm.build_error_internal(
        JS_TYPE_ERROR, u"Object.hasOwn called on non-object"));
  }
  return JSObjectPrototype::hasOwnProperty(vm, func, args[0], args.subarray(1, 1), flags);
}

inline ErrorOr<JSObject *> check_argument_and_get_object(NjsVM& vm, ArrayRef<JSValue> args) {
  JSValue arg;
  if (args.size() >= 1) [[likely]] {
    arg = args[0];
  }
  JSObject *obj;
  if (arg.is_object()) [[likely]] {
    obj = arg.as_object;
  } else {
    obj = TRY_ERR(js_to_object(vm, arg)).as_object;
  }
  return obj;
}

inline Completion Object_getPrototypeOf(vm_func_This_args_flags) {
  JSObject *obj = TRY_COMP(check_argument_and_get_object(vm, args));
  return obj->get_proto();
}

inline Completion Object_isExtensible(vm_func_This_args_flags) {
  JSObject *obj = TRY_COMP(check_argument_and_get_object(vm, args));
  return JSValue(obj->is_extensible());
}

inline Completion Object_preventExtensions(vm_func_This_args_flags) {
  JSObject *obj = TRY_COMP(check_argument_and_get_object(vm, args));
  obj->prevent_extensions();
  return undefined;
}

inline Completion Object_create(vm_func_This_args_flags) {
  JSValue arg;
  if (args.size() >= 1) [[likely]] {
    arg = args[0];
  }
  if (arg.is_null() || arg.is_object()) [[likely]] {
    JSObject *obj = vm.new_object(CLS_OBJECT, arg);
    return JSValue(obj);
  } else {
    return CompThrow(vm.build_error_internal(JS_TYPE_ERROR,
                                             u"Object prototype may only be an Object or null"));
  }
}

inline Completion Object_assign(vm_func_This_args_flags) {
  JSValue arg1;
  if (args.size() > 0) {
    arg1 = args[0];
  }

  TRYCC(js_require_object_coercible(vm, arg1));
  if (not arg1.is_object()) [[unlikely]] {
    arg1 = TRYCC(js_to_object(vm, arg1));
  }

  if (args.size() < 2) [[unlikely]] {
    return arg1;
  }
  JSObject *target_obj = args[0].as_object;

  for (int i = 1; i < args.size(); i++) {
    if (not args[i].is_object()) continue;
    JSObject *arg_obj = args[i].as_object;

    for (auto& [key, prop_desc] : arg_obj->get_storage()) {
      if (not prop_desc.flag.enumerable) continue;
      JSValue key_val(key.type);
      key_val.as_atom = key.atom;

      auto res = target_obj->set_prop(vm, key_val, TRYCC(arg_obj->get_prop(vm, key_val)));
      if (res.is_error()) return CompThrow(res.get_error());
    }
  }
  return arg1;
}

}

#endif // NJS_OBJECT_STATIC_METHOD_H
