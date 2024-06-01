#include "NativeFunction.h"
#include "njs/basic_types/JSValue.h"
#include "njs/common/AtomPool.h"
#include "njs/utils/macros.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/JSBoolean.h"
#include "njs/basic_types/JSNumber.h"
#include "njs/basic_types/JSString.h"
#include "njs/basic_types/JSRegExp.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSDate.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/qjs_date.h"

namespace njs {

Completion NativeFunction::Object_ctor(vm_func_This_args_flags) {
  if (args.size() == 0 || args[0].is_nil()) {
    return JSValue(vm.new_object());
  } else {
    return js_to_object(vm, args[0]);
  }
}

Completion NativeFunction::Number_ctor(vm_func_This_args_flags) {
  double num;
  if (args.size() == 0 || args[0].is_nil()) [[unlikely]] {
    num = 0;
  } else {
    num = TRY_COMP(js_to_number(vm, args[0]));
  }
  return JSValue(vm.heap.new_object<JSNumber>(vm, num));
}

Completion NativeFunction::String_ctor(vm_func_This_args_flags) {
  JSValue str;
  if (args.size() == 0 || args[0].is_nil()) [[unlikely]] {
    str = vm.get_string_const(AtomPool::k_);
  } else {
    str = TRY_COMP(js_to_string(vm, args[0]));
  }
  auto *obj = vm.heap.new_object<JSString>(vm, *str.val.as_prim_string);
  return JSValue(obj);
}


Completion NativeFunction::Array_ctor(vm_func_This_args_flags) {
  u32 length;
  if (args.size() == 0) [[likely]] {
    length = 0;
  } else {
    length = TRY_COMP(js_to_uint32(vm, args[0]));
  }
  JSArray *arr = vm.heap.new_object<JSArray>(vm, length);
  return JSValue(arr);
}

Completion NativeFunction::RegExp_ctor(vm_func_This_args_flags) {
  // TODO
  assert(args.size() > 0);
  JSValue pattern = TRY_COMP(js_to_string(vm, args[0]));
  u16string reflags;
  if (args.size() > 1) {
    reflags = TRY_COMP(js_to_string(vm, args[1])).val.as_prim_string->str;
  }

  return JSRegExp::New(vm, pattern.val.as_prim_string->str, reflags);
}


Completion NativeFunction::Date_ctor(vm_func_This_args_flags) {
  // call with `new`
  if (flags.this_is_new_target && This.val.as_func != nullptr) {
    auto num_arg = args.size();
    auto *date = vm.heap.new_object<JSDate>(vm);

    if (num_arg == 0) {
      date->set_to_now();
    }
    else if (num_arg == 1) {
      JSValue arg = args[1];
      if (arg.is_float64()) {
        date->timestamp = arg.val.as_f64;
      }
      else if (arg.is_object()) {
        date->timestamp = arg.as_object<JSDate>()->timestamp;
      }
      else {
        JSValue ts_str = TRY_COMP(js_to_string(vm, arg));
        date->parse_date_str(ts_str.val.as_prim_string->str);
      }
    } else {
      // TODO: other cases
      assert(false);
    }
    return JSValue(date);
  }
  // ignore arguments and always return a string representing current date.
  else {
    double ts = JSDate::get_curr_millis();
    u16string date_str = get_date_string(ts, 0x13);
    return vm.new_primitive_string(std::move(date_str));
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
    return CompThrow(err);
  }

  if (args.size() > 0 && not args[0].is_undefined()) {
    auto& str = TRY_COMP(js_to_string(vm, args[0])).val.as_prim_string->str;
    return JSSymbol(vm.atom_pool.atomize_symbol_desc(str));
  } else {
    return JSSymbol(vm.atom_pool.atomize_symbol());
  }
}

}