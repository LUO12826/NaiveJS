#ifndef NJS_JSVALUE_H
#define NJS_JSVALUE_H

#include <cstdint>
#include <string>
#include "RCObject.h"

namespace njs {

class JSObject;
class GCObject;
struct JSHeapValue;

extern const char *js_value_tag_names[22];

struct JSValue {
  
  enum JSValueTag {
    // The following types of values are stored inline in JSValue
    UNDEFINED,
    JS_ATOM,
    JS_NULL,
    BOOLEAN,
    NUMBER_INT,
    NUMBER_FLOAT,

    // Strings and Symbols are stored as pointers in JSValue
    STRING,
    SYMBOL,

    // Used when we wrap those inline values into JSHeapValue and hold a pointer to it.
    HEAP_VAL_REF,
    // Used when a STRING is considered shared. That is, when being assigned, instead of making
    // the pointer(PrimitiveString *) point to a new String, we just change the data in pointee.
    STRING_REF,
    // Used when a SYMBOL is considered shared.
    SYMBOL_REF,

    STACK_FRAME_META1,
    STACK_FRAME_META2,
    OTHER,

    NEED_GC_BEGIN,

    BOOLEAN_OBJ,
    NUMBER_OBJ,
    STRING_OBJ,
    OBJECT,
    ARRAY,
    FUNCTION,

    NEED_GC_END
  };

  static JSValue undefined;
  static JSValue null;

  JSValue(): tag(JSValueTag::UNDEFINED) {}
  explicit JSValue(JSValueTag tag): tag(tag) {}

  ~JSValue();

  explicit JSValue(double number): tag(NUMBER_FLOAT) {
    val.as_float64 = number;
  }

  explicit JSValue(int64_t number): tag(NUMBER_INT) {
    val.as_int = number;
  }

  explicit JSValue(bool boolean): tag(BOOLEAN) {
    val.as_bool = boolean;
  }

  explicit JSValue(JSObject *obj): tag(OBJECT) {
    val.as_object = obj;
  }

  explicit JSValue(std::u16string str): tag(STRING) {
    PrimitiveString *new_str = new PrimitiveString(std::move(str));
    new_str->retain();
    val.as_primitive_string = new_str;
  }

  inline bool is_int() const { return tag == NUMBER_INT; }
  inline bool is_float() const { return tag == NUMBER_FLOAT; }
  inline bool is_bool() { return tag == BOOLEAN; }
  inline bool is_primitive_string() { return tag == STRING; }
  inline bool is_object() { return tag == OBJECT; }

  inline bool is_RCObject() {
    return tag == HEAP_VAL_REF || tag == STRING_REF || tag == SYMBOL_REF;
  }

  inline bool needs_gc() const {
    return (tag >= NEED_GC_BEGIN) && (tag <= NEED_GC_END);
  }

  GCObject *as_GCObject() const;

  JSValue add(JSValue& rhs);

  std::string description() {
    std::string res = "JSValue tagged: " + std::string(js_value_tag_names[tag]);
    if (tag == BOOLEAN) res += ", value: " + std::to_string(val.as_bool);
    else if (tag == NUMBER_FLOAT) res += ", value: " + std::to_string(val.as_float64);
    else if (tag == NUMBER_INT) res += ", value: " + std::to_string(val.as_int);

    return res;
  }

  union {
    double as_float64;
    int64_t as_int;
    bool as_bool;

    PrimitiveString *as_primitive_string;
    JSHeapValue *as_heap_val;
    JSSymbol *as_symbol;

    JSObject *as_object;
  } val;

  JSValueTag tag;
  u32 flag_bits;
};

struct JSHeapValue: public RCObject {
  JSValue wrapped_val;
};

} // namespace njs

#endif // NJS_JSVALUE_H