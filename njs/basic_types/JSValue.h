#ifndef NJS_JSVALUE_H
#define NJS_JSVALUE_H

#include <cstdint>
#include <string>
#include "RCObject.h"
#include "PrimitiveString.h"

namespace njs {

class NjsVM;
class JSObject;
class GCObject;
class JSFunction;
class JSArray;
class JSString;
struct JSHeapValue;
struct JSSymbol;

using std::u16string;

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
/// or to a non-temporary storage such as object properties or array elements, `assign`
/// must be called to handle reference counting correctly.
struct JSValue {
  // has corresponding string representation, note to modify when adding
  enum JSValueTag {
    // The following types of values are stored inline in JSValue
    UNDEFINED = 0,
    // Before a `let` defined variable get initialized, its tag is UNINIT.
    UNINIT,

    JS_NULL,

    JS_ATOM,
    BOOLEAN,

    NUM_UINT32,
    NUM_INT32,
    NUM_INT64,
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

    NEED_RC_END,

    // These tags exist because:
    // 1. the runtime stack of the VM is a `JSValue[]` array
    // 2. when doing function calls, we have to save some metadata on the stack ( so that when the
    // function returns, we can restore the `pc`, the `frame_base_ptr` and other states.
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
  static JSValue uninited;
  static JSValue null;

  static JSValue Atom(int64_t val) {
    JSValue atom(JS_ATOM);
    atom.val.as_i64 = val;
    return atom;
  }

  JSValue(): JSValue(JSValueTag::UNDEFINED) {}
  explicit JSValue(JSValueTag tag): tag(tag) {}

  ~JSValue() = default;

  // Not trying to move anything here, since moving and copying cost are the same for JSValue.
  // Just to utilize the semantics of std::move to set the source value to undefined when it's moved.
  JSValue(JSValue&& other) {
    *this = other;
    other.tag = UNDEFINED;
  }

  JSValue(const JSValue& other) {
    *this = other;
  }

  JSValue& operator = (const JSValue& other) = default;

  JSValue& operator = (JSValue&& other) noexcept {
    val = other.val;
    tag = other.tag;
    flag_bits = other.flag_bits;
    other.tag = UNDEFINED;
    return *this;
  }

  explicit JSValue(double number): tag(NUM_FLOAT) {
    val.as_f64 = number;
  }

  explicit JSValue(int64_t number): tag(NUM_INT64) {
    val.as_i64 = number;
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

  void set_val(double number) {
    tag = NUM_FLOAT;
    val.as_f64 = number;
  }

  void set_val(int64_t number) {
    tag = NUM_INT64;
    val.as_i64 = number;
  }

  void set_val(bool boolean) {
    tag = BOOLEAN;
    val.as_bool = boolean;
  }

  void set_val(JSValue *js_val) {
    tag = VALUE_HANDLE;
    val.as_JSValue = js_val;
  }

  void set_val(JSObject *obj) {
    tag = OBJECT;
    val.as_object = obj;
  }

  void set_val(PrimitiveString *str) {
    tag = STRING;
    val.as_primitive_string = str;
  }

  void set_val(JSArray *array) {
    tag = ARRAY;
    val.as_array = array;
  }

  void set_val(JSFunction *func) {
    tag = FUNCTION;
    val.as_function = func;
  }

  /// Use this method to destroy a temporary value (in general, a temporary value
  /// is a value on the operand stack)
  void set_undefined() {
    if (is_RCObject() && val.as_RCObject->get_ref_count() == 0) {
      val.as_RCObject->delete_temp_object();
    }
    tag = UNDEFINED;
  }

  /// Use this method to destroy a in-memory value (the memory here is NjsVM's memory, i.e.,
  /// function call stack and GCHeap.
  void dispose() {
    if (is_RCObject()) val.as_RCObject->release();
    tag = UNDEFINED;
  }

  JSValue& deref() const;
  JSValue& deref_heap() const;

  void move_to_heap();

  bool is_undefined() const { return tag == UNDEFINED; };
  bool is_uninited() const { return tag == UNINIT; };
  bool is_null() const { return tag == JS_NULL; };
  bool is_int64() const { return tag == NUM_INT64; }
  bool is_float64() const { return tag == NUM_FLOAT; }
  bool is_bool() const { return tag == BOOLEAN; }
  bool is_primitive_string() const { return tag == STRING; }
  bool is_inline() const { return tag >= JS_ATOM && tag <= NUM_FLOAT; }

  /// @brief NOTE: this method does not check the tag.
  bool is_integer() const {
    return double(int64_t(val.as_f64)) == val.as_f64;
  }

  /// @brief NOTE: this method does not check the tag.
  bool is_non_negative() const {
    return val.as_f64 >= 0;
  }

  bool is_string_type() const {
    return tag == JS_ATOM || tag == STRING || tag == STRING_OBJ;
  }

  bool is_number_type() const {
    return tag >= NUM_UINT32 && tag <= NUM_FLOAT;
  }

  bool is_object() const {
    return tag >= NEED_GC_BEGIN && tag <= NEED_GC_END;
  }

  bool is_function() const {
    return tag == FUNCTION;
  }

  bool is_RCObject() const {
    return tag > NEED_RC_BEGIN && tag < NEED_RC_END;
  }

  bool needs_gc() const {
    return tag >= NEED_GC_BEGIN && tag <= NEED_GC_END;
  }

  GCObject *as_GCObject() const;
  JSObject *as_object() const {
    return val.as_object;
  }

  double as_f64() const {
    switch (tag) {
      case NUM_FLOAT:
        return val.as_f64;
      case NUM_INT64:
        return double(val.as_i64);
      case NUM_INT32:
        return double(val.as_i32);
      case NUM_UINT32:
        return double(val.as_u32);
      default:
        assert(false);
    }
  }

  bool is_falsy() const {
    if (tag == BOOLEAN) return !val.as_bool;
    if (tag == JS_NULL || tag == UNDEFINED) return true;
    if (tag == NUM_FLOAT) return val.as_f64 == 0 || std::isnan(val.as_f64);
    if (tag == STRING) return val.as_primitive_string->str.empty();
    return false;
  }

  bool bool_value() const {
    return !is_falsy();
  }

  bool is(JSValueTag val_tag) const {
    return tag == val_tag;
  }

  void assign(const JSValue& rhs) {
    assert(tag != STACK_FRAME_META1 && tag != STACK_FRAME_META2);
    assert(rhs.tag != STACK_FRAME_META1 && rhs.tag != STACK_FRAME_META2);

    if (rhs.tag == tag && rhs.flag_bits == flag_bits && rhs.val.as_i64 == val.as_i64) return;

    // if rhs is an RC object, we are going to retain the new object
    if (rhs.is_RCObject()) {
      rhs.val.as_RCObject->retain();
    }

    // if this is an RC object, we are going to release the old object
    if (is_RCObject()) {
      val.as_RCObject->release();
    }
    // copy
    val.as_i64 = rhs.val.as_i64;
    flag_bits = rhs.flag_bits;
    tag = rhs.tag;
  }

  /// `description()` is for internal debugging.
  std::string description() const;
  /// `to_string(NjsVM&)` is for console.log.
  std::string to_string(NjsVM& vm) const;
  /// `to_json(u16string&, NjsVM&)` is for JSON.stringify.
  void to_json(u16string& output, NjsVM& vm) const;

  union {
    double as_f64;
    int64_t as_i64;
    uint32_t as_u32;
    int32_t as_i32;
    bool as_bool;

    JSValue *as_JSValue;

    RCObject *as_RCObject;
    JSSymbol *as_symbol;
    PrimitiveString *as_primitive_string;
    JSHeapValue *as_heap_val;

    JSObject *as_object;
    JSArray *as_array;
    JSString *as_string;
    JSFunction *as_function;
  } val;

  JSValueTag tag;
  u32 flag_bits {0};
};

inline JSValue& JSValue::deref() const {
  assert(tag == VALUE_HANDLE);
  return *val.as_JSValue;
}

} // namespace njs

#endif // NJS_JSVALUE_H