
#include "NativeFunction.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/JSBoolean.h"
#include "njs/basic_types/JSNumber.h"
#include "njs/basic_types/JSString.h"
#include "njs/common/common_def.h"

namespace njs {

Completion NativeFunctions::Object_ctor(vm_func_This_args_flags) {
  if (args.size() == 0 || args[0].is_null() || args[0].is_undefined()) {
    return JSValue(vm.new_object());
  }
  JSValue arg = args[0];

  if (arg.is_object()) return arg;

  JSObject *obj;
  switch (arg.tag) {
    case JSValue::BOOLEAN:
      obj = vm.heap.new_object<JSBoolean>(vm, arg.val.as_bool);
      break;
    case JSValue::NUM_FLOAT:
      obj = vm.heap.new_object<JSNumber>(vm, arg.val.as_f64);
      break;
    case JSValue::STRING:
      obj = vm.heap.new_object<JSString>(vm, *arg.val.as_prim_string);
      break;
    default:
      assert(false);
  }

  return JSValue(obj);
}

Completion NativeFunctions::error_ctor_internal(NjsVM& vm, ArrayRef<JSValue> args, JSErrorType type) {
  auto *err_obj = vm.new_object(ObjectClass::CLS_ERROR, vm.native_error_protos[type]);
  if (args.size() > 0 && args[0].is_string_type()) {
    // only supports primitive string now.
    assert(args[0].is(JSValue::STRING));
    err_obj->add_prop(vm, u"message", args[0]);
  }

  u16string trace_str = NativeFunctions::build_trace_str(vm, true);
  err_obj->add_prop(vm, u"stack", vm.new_primitive_string(std::move(trace_str)));

  return JSValue(err_obj);
}

Completion NativeFunctions::Symbol(vm_func_This_args_flags) {
  if (flags.this_is_new_target && !This.is_undefined()) {
    JSValue err = NativeFunctions::build_error_internal(vm, JS_TYPE_ERROR, u"Symbol() is not a constructor.");
    return Completion::with_throw(err);
  }

  if (args.size() > 0 && not args[0].is_undefined()) {
    assert(args[0].tag == JSValue::STRING);
    // TODO: do a `ToString` here.
    auto& str = args[0].val.as_prim_string->str;
    return JSValue::Symbol(vm.str_pool.atomize_symbol_desc(str));
  } else {
    return JSValue::Symbol(vm.str_pool.atomize_symbol());
  }
}

}