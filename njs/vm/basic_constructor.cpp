#include "native.h"
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
#include "njs/basic_types/JSPromise.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/qjs_date.h"

namespace njs::native {

Completion ctor::Object(vm_func_This_args_flags) {
  if (args.empty() || args[0].is_nil()) {
    return JSValue(vm.new_object());
  } else {
    return js_to_object(vm, args[0]);
  }
}

Completion ctor::Number(vm_func_This_args_flags) {
  double num;
  if (args.empty() || args[0].is_nil()) [[unlikely]] {
    num = 0;
  } else {
    num = TRY_COMP(js_to_number(vm, args[0]));
  }
  return JSValue(vm.heap.new_object<JSNumber>(vm, num));
}

Completion ctor::String(vm_func_This_args_flags) {
  JSValue str;
  if (args.empty() || args[0].is_nil()) [[unlikely]] {
    str = vm.get_string_const(AtomPool::k_);
  } else {
    str = TRYCC(js_to_string(vm, args[0]));
  }
  auto *obj = vm.heap.new_object<JSString>(vm, str.as_prim_string);
  return JSValue(obj);
}


Completion ctor::Array(vm_func_This_args_flags) {
  u32 length;
  if (args.empty()) [[likely]] {
    length = 0;
  } else {
    length = TRY_COMP(js_to_uint32(vm, args[0]));
  }
  JSArray *arr = vm.heap.new_object<JSArray>(vm, length);
  return JSValue(arr);
}


Completion ctor::RegExp(vm_func_This_args_flags) {
  NOGC;
  u16string_view pattern;
  if (args.size() > 0) {
    pattern = TRYCC(js_to_string(vm, args[0])).as_prim_string->view();
  }
  u16string_view reflags;
  if (args.size() > 1) {
    reflags = TRYCC(js_to_string(vm, args[1])).as_prim_string->view();
  }

  return JSRegExp::New(vm, pattern, reflags);
}


Completion ctor::Date(vm_func_This_args_flags) {
  NOGC;
  // call with `new`
  if (flags.this_is_new_target) {
    auto num_arg = args.size();
    auto *date = vm.heap.new_object<JSDate>(vm);

    if (num_arg == 0) {
      date->set_to_now();
    }
    else if (num_arg == 1) {
      JSValue arg = args[0];
      if (arg.is_float64()) {
        date->timestamp = arg.as_f64;
      } else {
        JSValue ts_str = TRYCC(js_to_string(vm, arg));
        date->parse_date_str(ts_str.as_prim_string->view());
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
    return vm.new_primitive_string(date_str);
  }
}

Completion ctor::Function(vm_func_This_args_flags) {
  u32 key = vm.str_to_atom(u"___dummy");
  return vm.global_object.as_object->get_prop_trivial(key);
}

Completion ctor::Promise(vm_func_This_args_flags) {
  if (not flags.this_is_new_target) {
    return vm.throw_error(JS_TYPE_ERROR,
                          u"Promise constructor cannot be invoked without 'new'");
  }
  if (args.empty() || not args[0].is_function()) {
    return vm.throw_error(JS_TYPE_ERROR,
                          u"Promise resolver is not a function");
  }
  return JSPromise::New(vm, args[0]);
}

// TODO
Completion ctor::GeneratorFunction(vm_func_This_args_flags) {
  return undefined;
}

Completion ctor::error_ctor_internal(NjsVM& vm, ArgRef args, JSErrorType type) {
  auto *err_obj = vm.new_object(CLS_ERROR, vm.native_error_protos[type]);
  if (!args.empty() && args[0].is_string_type()) {
    // only supports primitive string now.
    assert(args[0].is_prim_string());
    err_obj->set_prop(vm, u"message", args[0]);
  }

  u16string trace_str = vm.build_trace_str(true);
  err_obj->set_prop(vm, u"stack", vm.new_primitive_string(trace_str));

  return JSValue(err_obj);
}

Completion ctor::Symbol(vm_func_This_args_flags) {
  if (flags.this_is_new_target) {
    return vm.throw_error(JS_TYPE_ERROR, u"Symbol() is not a constructor.");
  }

  if (!args.empty() && not args[0].is_undefined()) {
    auto str = TRYCC(js_to_string(vm, args[0])).as_prim_string->view();
    return JSSymbol(vm.atom_pool.atomize_symbol_desc(str));
  } else {
    return JSSymbol(vm.atom_pool.atomize_symbol());
  }
}

}