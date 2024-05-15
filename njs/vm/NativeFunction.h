#ifndef NJS_NATIVE_FUNCTION_H
#define NJS_NATIVE_FUNCTION_H

#include "njs/basic_types/JSValue.h"
#include "njs/vm/Completion.h"
#include "njs/common/ArrayRef.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/JSErrorPrototype.h"
namespace njs {

class NjsVM;

class NativeFunction {
 public:
  static Completion log(JS_NATIVE_FUNC_PARAMS);
  static Completion debug_log(JS_NATIVE_FUNC_PARAMS);
  static Completion js_gc(JS_NATIVE_FUNC_PARAMS);
  static Completion set_timeout(JS_NATIVE_FUNC_PARAMS);
  static Completion set_interval(JS_NATIVE_FUNC_PARAMS);
  static Completion clear_interval(JS_NATIVE_FUNC_PARAMS);
  static Completion clear_timeout(JS_NATIVE_FUNC_PARAMS);
  static Completion fetch(JS_NATIVE_FUNC_PARAMS);
  static Completion json_stringify(JS_NATIVE_FUNC_PARAMS);

  static Completion Object_ctor(JS_NATIVE_FUNC_PARAMS);
  static Completion error_ctor_internal(NjsVM& vm, ArrayRef<JSValue> args, JSErrorType type);

  static Completion Symbol(JS_NATIVE_FUNC_PARAMS);

};

}

#endif // NJS_NATIVE_FUNCTION_H
