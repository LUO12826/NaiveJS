#ifndef NJS_JSVALUE_H
#define NJS_JSVALUE_H

#include <cstdint>
#include <string>
#include "RCObject.h"

namespace njs {

class JSObject;
class GCObject;

struct PrimitiveString: public RCObject {

  PrimitiveString(std::u16string str): str(std::move(str)) {}

  std::u16string str;
};

struct JSValue {
  
  enum JSValueTag {
    UNDEFINED,
    JS_ATOM,
    JS_NULL,
    BOOLEAN,
    NUMBER_INT,
    NUMBER_FLOAT,
    STRING,

    VALUE_REF,

    NEED_GC_BEGIN,

    BOOLEAN_OBJ,
    NUMBER_OBJ,
    STRING_OBJ,
    OBJECT,
    ARRAY,
    FUNCTION,

    NEED_GC_END
  };

  JSValue(double number): tag(NUMBER_FLOAT) {
    val.as_float64 = number;
  }

  JSValue(int64_t number): tag(NUMBER_INT) {
    val.as_int = number;
  }

  JSValue(bool boolean): tag(BOOLEAN) {
    val.as_bool = boolean;
  }

  JSValue(std::u16string str): tag(STRING) {
    PrimitiveString *new_str = new PrimitiveString(std::move(str));
    new_str->retain();
    val.as_primitive_string = new_str;
  }

  inline bool is_int() { return tag == NUMBER_INT; }
  inline bool is_float() { return tag == NUMBER_FLOAT; }
  inline bool is_bool() { return tag == BOOLEAN; }
  inline bool is_primitive_string() { return tag == STRING; }
  inline bool is_object() { return tag == OBJECT; }

  GCObject *as_GCObject();

  JSValue add(JSValue& rhs);

  union {
    double as_float64;
    int64_t as_int;
    bool as_bool;

    void *as_ptr;
    PrimitiveString *as_primitive_string;
    JSObject *as_object;
  } val;

  JSValueTag tag;
};

} // namespace njs

#endif // NJS_JSVALUE_H