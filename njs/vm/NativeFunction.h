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

  static Completion Object_ctor(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static Completion Error_ctor(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);

  static u16string build_trace_str(NjsVM& vm);
  static JSValue build_error_internal(NjsVM& vm, const u16string& msg);

};

}

#endif // NJS_NATIVE_FUNCTION_H
