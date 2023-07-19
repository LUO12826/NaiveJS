#ifndef NJS_JSVALUE_H
#define NJS_JSVALUE_H

#include <cstdint>
#include <string>
#include "RCObject.h"

namespace njs {

class JSObject;
class GCObject;
class JSFunction;
class JSArray;
struct JSHeapValue;

extern const char *js_value_tag_names[25];

/// Type for values in JavaScript
/// JSValue may point to an object that manages memory by reference counting,
/// so be especially careful when copying and destroying.
///
/// Here are some basic principles: JSValues on the operand stack are temporary values,
/// so their reference count should not be changed when they're moved or copied. But when
/// they disappears, `set_undefined` should be called in order to clear the temporary value
/// and avoid memory leaks.
///
/// When a JSValue is transferred from the operand stack to somewhere on the stack frame,
/// or to a non-temporary storage such as object attributes or array elements, `assign`
/// must be called to handle reference counting correctly.
struct JSValue {
  // has corresponding string representation, note to modify when adding
  enum JSValueTag {
    // The following types of values are stored inline in JSValue
    UNDEFINED,
    JS_ATOM,
    JS_NULL,
    BOOLEAN,
    NUM_INT,
    NUM_FLOAT,

    // A reference to another JSValue, no lifecycle considerations
    // Currently only used when need to assign a value to an object's property
    VALUE_HANDLE,

    NEED_RC_BEGIN,

    // Strings and Symbols are stored as pointers in JSValue
    STRING,
    SYMBOL,

    // Used when we wrap those inline values into JSHeapValue and hold a pointer to it.
    // Currently, turning a variable into a closure variable will turn it into a JSHeapValue
    HEAP_VAL,
    // Used when a STRING is considered shared. That is, when being assigned, instead of making
    // the pointer(PrimitiveString *) point to a new String, we just change the data in pointee.
    STRING_REF,
    // Used when a SYMBOL is considered shared.
    SYMBOL_REF,

    NEED_RC_END,

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
  ~JSValue() = default;

  // Not trying to move anything here, since moving and copying cost the same for JSValue.
  // Just to utilize the semantics of std::move to set the source value to undefined when it's moved.
  JSValue(JSValue&& other) {
    *this = other;
    other.tag = UNDEFINED;
  }

  JSValue(const JSValue& other) {
    *this = other;
  }

  JSValue& operator = (const JSValue& other) {
    if (this == &other) return *this;
    val = other.val;
    tag = other.tag;
    flag_bits = other.flag_bits;
    return *this;
  }

  JSValue& operator = (JSValue&& other) {
    if (this == &other) return *this;
    val = other.val;
    tag = other.tag;
    flag_bits = other.flag_bits;
    other.tag = UNDEFINED;
    return *this;
  }

  explicit JSValue(double number): tag(NUM_FLOAT) {
    val.as_float64 = number;
  }

  explicit JSValue(int64_t number): tag(NUM_INT) {
    val.as_int64 = number;
  }

  explicit JSValue(bool boolean): tag(BOOLEAN) {
    val.as_bool = boolean;
  }

  explicit JSValue(JSValue *js_val): tag(VALUE_HANDLE) {
    val.as_JSValue = js_val;
  }

  explicit JSValue(JSObject *obj): tag(OBJECT) {
    val.as_object = obj;
  }

  explicit JSValue(PrimitiveString *str): tag(STRING) {
    val.as_primitive_string = str;
  }

  explicit JSValue(JSArray *array): tag(ARRAY) {
    val.as_array = array;
  }

  explicit JSValue(JSFunction *func): tag(FUNCTION) {
    val.as_function = func;
  }

  /// Use this method to destroy a temporary value (in general, a temporary value
  /// is a value on the operand stack)
  inline void set_undefined() {
    if (is_RCObject() && val.as_RCObject->get_ref_count() == 0) {
      val.as_RCObject->delete_temp_object();
    }
    tag = UNDEFINED;
  }

  /// Use this method to destroy a in-memory value (the memory here is NjsVM's memory, i.e.,
  /// function call stack and GCHeap.
  inline void dispose() {
    if (is_RCObject()) val.as_RCObject->release();
    tag = UNDEFINED;
  }

  JSValue& deref();
  JSValue& deref_if_needed();

  void move_to_heap();

  inline bool is_int() const { return tag == NUM_INT; }
  inline bool is_float() const { return tag == NUM_FLOAT; }
  inline bool is_bool() const { return tag == BOOLEAN; }
  inline bool is_primitive_string() const { return tag == STRING; }
  inline bool is_object() const {
    return tag == OBJECT || tag == FUNCTION || tag == ARRAY;
  }

  inline bool is_RCObject() const {
    return tag > NEED_RC_BEGIN && tag < NEED_RC_END;
  }

  inline bool needs_gc() const {
    return (tag >= NEED_GC_BEGIN) && (tag <= NEED_GC_END);
  }

  GCObject *as_GCObject() const;

  bool is_falsy() const;
  bool bool_value() const;

  bool tag_is(JSValueTag val_tag) const;

  void assign(const JSValue& rhs);

  std::string description() const;
  std::string to_string() const;

  union {
    double as_float64;
    int64_t as_int64;
    bool as_bool;

    JSValue *as_JSValue;

    RCObject *as_RCObject;
    JSSymbol *as_symbol;
    PrimitiveString *as_primitive_string;
    JSHeapValue *as_heap_val;

    JSObject *as_object;
    JSArray *as_array;
    JSFunction *as_function;
  } val;

  JSValueTag tag;
  u32 flag_bits;
};

struct JSHeapValue: public RCObject {
  JSHeapValue(JSValue val): wrapped_val(val) {}
  ~JSHeapValue() {
    wrapped_val.dispose();
  }

  JSValue wrapped_val;
};

} // namespace njs

#endif // NJS_JSVALUE_H