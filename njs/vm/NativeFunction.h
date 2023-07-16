#ifndef NJS_NATIVE_FUNCTION_H
#define NJS_NATIVE_FUNCTION_H

#include "njs/basic_types/JSValue.h"
#include "njs/common/ArrayRef.h"


namespace njs {

class JSObject;
class NjsVM;

class InternalFunctions {
 public:
  static JSValue log(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
  static JSValue js_gc(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args);
};

}

#endif // NJS_NATIVE_FUNCTION_H
