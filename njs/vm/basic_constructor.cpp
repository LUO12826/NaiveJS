#include "NativeFunction.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/JSBoolean.h"
#include "njs/basic_types/JSNumber.h"
#include "njs/basic_types/JSString.h"
#include "njs/common/common_def.h"

namespace njs {

Completion NativeFunction::Object_ctor(vm_func_This_args_flags) {
  if (args.size() == 0 || args[0].is_nil()) {
    return JSValue(vm.new_object());
  } else {
    return js_to_object(vm, args[0]);
  }
}

Completion NativeFunction::error_ctor_internal(NjsVM& vm, ArrayRef<JSValue> args, JSErrorType type) {
  auto *err_obj = vm.new_object(CLS_ERROR, vm.native_error_protos[type]);
  if (args.size() > 0 && args[0].is_string_type()) {
    // only supports primitive string now.
    assert(args[0].is_prim_string());
    err_obj->set_prop(vm, u"message", args[0]);
  }

  u16string trace_str = vm.build_trace_str(true);
  err_obj->set_prop(vm, u"stack", vm.new_primitive_string(std::move(trace_str)));

  return JSValue(err_obj);
}

Completion NativeFunction::Symbol(vm_func_This_args_flags) {
  if (flags.this_is_new_target && !This.is_undefined()) {
    JSValue err = vm.build_error_internal(JS_TYPE_ERROR, u"Symbol() is not a constructor.");
    return Completion::with_throw(err);
  }

  if (args.size() > 0 && not args[0].is_undefined()) {
    assert(args[0].tag == JSValue::STRING);
    // TODO: do a `ToString` here.
    auto& str = args[0].val.as_prim_string->str;
    return JSSymbol(vm.atom_pool.atomize_symbol_desc(str));
  } else {
    return JSSymbol(vm.atom_pool.atomize_symbol());
  }
}

}