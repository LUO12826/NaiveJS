#ifndef NJS_NATIVE_FUNCTION_H
#define NJS_NATIVE_FUNCTION_H

#include <random>

#include "njs/basic_types/JSValue.h"
#include "njs/common/Completion.h"
#include "njs/common/Span.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/JSErrorPrototype.h"


namespace njs::native {
using njs::NjsVM;

struct misc {
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
};

struct ctor {
  static Completion Object(vm_func_This_args_flags);
  static Completion Number(vm_func_This_args_flags);
  static Completion String(vm_func_This_args_flags);
  static Completion Array(vm_func_This_args_flags);
  static Completion RegExp(vm_func_This_args_flags);
  static Completion Date(vm_func_This_args_flags);
  static Completion Function(vm_func_This_args_flags);
  static Completion Promise(vm_func_This_args_flags);
  static Completion GeneratorFunction(vm_func_This_args_flags);
  static Completion error_ctor_internal(NjsVM& vm, Span<JSValue> args, JSErrorType type);
  // `Symbol` is actually not a constructor.
  static Completion Symbol(vm_func_This_args_flags);
};

struct Math {
  static Completion min(vm_func_This_args_flags);
  static Completion max(vm_func_This_args_flags);
  static Completion floor(vm_func_This_args_flags);
  static Completion random(vm_func_This_args_flags);
};

struct Object {
  static Completion defineProperty(vm_func_This_args_flags);
  static Completion hasOwn(vm_func_This_args_flags);
  static Completion getPrototypeOf(vm_func_This_args_flags);
  static Completion setPrototypeOf(vm_func_This_args_flags);
  static Completion create(vm_func_This_args_flags);
  static Completion assign(vm_func_This_args_flags);
  static Completion preventExtensions(vm_func_This_args_flags);
  static Completion isExtensible(vm_func_This_args_flags);
};

}


#endif // NJS_NATIVE_FUNCTION_H
