#ifndef NJS_JSVALUE_H
#define NJS_JSVALUE_H

#include <cstdint>

#include "njs/gc/GCObject.h"

namespace njs {

struct JSValue {
  
  enum JSValueTag {
    UNDEFINED,
    JS_ATOM,
    JS_NULL,
    BOOLEAN,
    NUMBER,
    STRING,

    BOOLEAN_OBJ,
    NUMBER_OBJ,
    STRING_OBJ,

    NEED_GC_BEGIN,
    OBJECT,
    ARRAY,
    FUNCTION,
    NEED_GC_END
  };

  union {
    double as_float64;
    void *as_ptr;
    int64_t as_int;
  } val;

  JSValueTag tag;

  GCObject *as_GCObject() { return static_cast<GCObject *>(val.as_ptr); }
};

} // namespace njs

#endif // NJS_JSVALUE_H