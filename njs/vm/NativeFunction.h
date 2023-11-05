#ifndef NJS_NATIVE_FUNCTION_H
#define NJS_NATIVE_FUNCTION_H

#include "njs/basic_types/JSValue.h"
#include "njs/common/ArrayRef.h"
namespace njs {

class NjsVM;

class InternalFunctions {
 public:
  static JSValue log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue debug_log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue js_gc(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue set_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue set_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue clear_interval(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue clear_timeout(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue fetch(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue json_stringify(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue error_ctor(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue error_build_internal(NjsVM& vm, const u16string& msg);
};

}

#endif // NJS_NATIVE_FUNCTION_H
