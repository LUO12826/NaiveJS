#ifndef NJS_NATIVE_FUNCTION_H
#define NJS_NATIVE_FUNCTION_H

#include "njs/basic_types/JSValue.h"
#include "njs/vm/Completion.h"
#include "njs/common/ArrayRef.h"
namespace njs {

class NjsVM;

class InternalFunctions {
 public:
  static Completion log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion debug_log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion js_gc(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion set_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion set_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion clear_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion clear_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion fetch(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion json_stringify(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion error_ctor(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue error_build_internal(NjsVM& vm, const u16string& msg);

  static Completion test_throw_err(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);

};

}

#endif // NJS_NATIVE_FUNCTION_H
