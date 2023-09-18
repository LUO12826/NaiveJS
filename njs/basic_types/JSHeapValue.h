#ifndef NJS_JS_HEAP_VALUE_H
#define NJS_JS_HEAP_VALUE_H

#include "RCObject.h"
#include "JSValue.h"

namespace njs {

struct JSHeapValue: public RCObject {
  explicit JSHeapValue(JSValue val): wrapped_val(val) {}
  ~JSHeapValue() override {
    wrapped_val.dispose();
  }

  RCObject *copy() override {
    return new JSHeapValue(wrapped_val);
  }

  JSValue wrapped_val;
};

}

#endif //NJS_JS_HEAP_VALUE_H
