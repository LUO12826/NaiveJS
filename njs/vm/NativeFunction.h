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
  static Completion log(JS_NATIVE_FUNC_PARAMS);
  static Completion debug_log(JS_NATIVE_FUNC_PARAMS);
  static Completion debug_trap(JS_NATIVE_FUNC_PARAMS);
  static Completion dummy(JS_NATIVE_FUNC_PARAMS);
  static Completion _test(JS_NATIVE_FUNC_PARAMS);
  static Completion js_gc(JS_NATIVE_FUNC_PARAMS);
  static Completion set_timeout(JS_NATIVE_FUNC_PARAMS);
  static Completion set_interval(JS_NATIVE_FUNC_PARAMS);
  static Completion clear_interval(JS_NATIVE_FUNC_PARAMS);
  static Completion clear_timeout(JS_NATIVE_FUNC_PARAMS);
  static Completion fetch(JS_NATIVE_FUNC_PARAMS);
  static Completion json_stringify(JS_NATIVE_FUNC_PARAMS);
  static Completion isFinite(JS_NATIVE_FUNC_PARAMS);
  static Completion parseFloat(JS_NATIVE_FUNC_PARAMS);
  static Completion parseInt(JS_NATIVE_FUNC_PARAMS);

  static Completion Object_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion Number_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion String_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion Array_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion RegExp_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion Date_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion Function_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion Promise_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion GeneratorFunction_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion error_ctor_internal(NjsVM& vm, Span<JSValue> args, JSErrorType type);

  static Completion Symbol(JS_NATIVE_FUNC_PARAMS);

};

class JSMath {
 public:
  static Completion min(JS_NATIVE_FUNC_PARAMS);
  static Completion max(JS_NATIVE_FUNC_PARAMS);
  static Completion floor(JS_NATIVE_FUNC_PARAMS);
  static Completion random(JS_NATIVE_FUNC_PARAMS);
};

}

#endif // NJS_NATIVE_FUNCTION_H
