#ifndef NJS_NATIVE_FUNCTION_H
#define NJS_NATIVE_FUNCTION_H

#include "njs/basic_types/JSValue.h"
#include "njs/common/Completion.h"
#include "njs/common/Span.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/JSErrorPrototype.h"
namespace njs {

class NjsVM;

class NativeFunction {
 public:
  static Completion log(vm_func_This_args_flags);
  static Completion debug_log(vm_func_This_args_flags);
  static Completion debug_trap(vm_func_This_args_flags);
  static Completion dummy(vm_func_This_args_flags);
  static Completion _test(vm_func_This_args_flags);
  static Completion js_gc(vm_func_This_args_flags);
  static Completion set_timeout(vm_func_This_args_flags);
  static Completion set_interval(vm_func_This_args_flags);
  static Completion clear_interval(vm_func_This_args_flags);
  static Completion clear_timeout(vm_func_This_args_flags);
  static Completion fetch(vm_func_This_args_flags);
  static Completion json_stringify(vm_func_This_args_flags);
  static Completion json_parse(vm_func_This_args_flags);
  static Completion isFinite(vm_func_This_args_flags);
  static Completion parseFloat(vm_func_This_args_flags);
  static Completion parseInt(vm_func_This_args_flags);

  static Completion Object_ctor(vm_func_This_args_flags);
  static Completion Number_ctor(vm_func_This_args_flags);
  static Completion String_ctor(vm_func_This_args_flags);
  static Completion Array_ctor(vm_func_This_args_flags);
  static Completion RegExp_ctor(vm_func_This_args_flags);
  static Completion Date_ctor(vm_func_This_args_flags);
  static Completion Function_ctor(vm_func_This_args_flags);
  static Completion Promise_ctor(vm_func_This_args_flags);
  static Completion GeneratorFunction_ctor(vm_func_This_args_flags);
  static Completion error_ctor_internal(NjsVM& vm, Span<JSValue> args, JSErrorType type);

  static Completion Symbol(vm_func_This_args_flags);

};

class JSMath {
 public:
  static Completion min(vm_func_This_args_flags);
  static Completion max(vm_func_This_args_flags);
  static Completion floor(vm_func_This_args_flags);
  static Completion random(vm_func_This_args_flags);
};

}

#endif // NJS_NATIVE_FUNCTION_H
