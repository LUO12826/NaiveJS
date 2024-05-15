#ifndef NJS_JSVALUE_H
#define NJS_JSVALUE_H

#include <cstdint>
#include <string>
#include <cassert>
#include "PrimitiveString.h"

namespace njs {

class NjsVM;
class JSObject;
class GCObject;
class JSFunction;
class JSArray;
class JSString;
struct JSHeapValue;

using std::u16string;
using u32 = uint32_t;

extern const char *js_value_tag_names[];

/// Type for values in JavaScript
struct JSValue {
friend class NjsVM;

  // has corresponding string representation, note to modify when adding
  enum JSValueTag {
    // The following types of values are stored inline in JSValue
    UNDEFINED = 0,
    // Before a `let` defined variable get initialized, its tag is UNINIT.
    UNINIT,
    JS_NULL,

    JS_ATOM,
    // symbols are always atomized.
    SYMBOL,
    BOOLEAN,

    NUM_UINT32,
    NUM_INT32,
    NUM_FLOAT,

    // A reference to another JSValue, no lifecycle considerations
    // Currently only used when need to assign a value to an object's property
    VALUE_HANDLE,

    NEED_GC_BEGIN,

    // Strings and Symbols are stored as pointers in JSValue
    STRING,

    // Used when we wrap those inline values into JSHeapValue and hold a pointer to it.
    // Currently, turning a variable into a closure variable will turn it into a JSHeapValue
    HEAP_VAL,

    OBJECT_BEGIN,

    BOOLEAN_OBJ,
    NUMBER_OBJ,
    STRING_OBJ,
    OBJECT,
    ARRAY,
    FUNCTION,

    NEED_GC_END,
    JSVALUE_TAG_CNT
  };

  static JSValue undefined;
  static JSValue uninited;
  static JSValue null;

  inline static JSValue Atom(u32 val) {
    JSValue atom(JS_ATOM);
    atom.val.as_atom = val;
    return atom;
  }

  inline static JSValue Symbol(u32 val) {
    JSValue symbol(SYMBOL);
    symbol.val.as_symbol = val;
    return symbol;
  }

  inline static JSValue U32(u32 val) {
    JSValue num(NUM_UINT32);
    num.val.as_u32 = val;
    return num;
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
    if (this == &other) return *this;
    val = other.val;
    tag = other.tag;
    flag_bits = other.flag_bits;
    other.tag = UNDEFINED;
    return *this;
  }

  explicit JSValue(double number): tag(NUM_FLOAT) {
    val.as_f64 = number;
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
    val.as_prim_string = str;
  }

  explicit JSValue(JSArray *array): tag(ARRAY) {
    val.as_array = array;
  }

  explicit JSValue(JSFunction *func): tag(FUNCTION) {
    val.as_func = func;
  }

  void set_val(double number) {
    tag = NUM_FLOAT;
    val.as_f64 = number;
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
    val.as_prim_string = str;
  }

  void set_val(JSArray *array) {
    tag = ARRAY;
    val.as_array = array;
  }

  void set_val(JSFunction *func) {
    tag = FUNCTION;
    val.as_func = func;
  }

  void set_undefined() { tag = UNDEFINED; }
  void set_uninited() { tag = UNINIT; }

  JSValue& deref() const;
  JSValue& deref_heap() const;

  void move_to_heap(NjsVM& vm);

  bool is_undefined() const { return tag == UNDEFINED; };
  bool is_uninited() const { return tag == UNINIT; };
  bool is_null() const { return tag == JS_NULL; };
  bool is_float64() const { return tag == NUM_FLOAT; }
  bool is_bool() const { return tag == BOOLEAN; }
  bool is_atom() const { return tag == JS_ATOM; }
  bool is_symbol() const { return tag == SYMBOL; }
  bool is_prim_string() const { return tag == STRING; }
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
    return tag == STRING || tag == STRING_OBJ;
  }

  bool is_number_type() const {
    return tag >= NUM_UINT32 && tag <= NUM_FLOAT;
  }

  bool is_object() const {
    return tag > OBJECT_BEGIN && tag < NEED_GC_END;
  }

  bool is_function() const {
    return tag == FUNCTION;
  }

  bool needs_gc() const {
    return tag > NEED_GC_BEGIN && tag < NEED_GC_END;
  }

  GCObject *as_GCObject() const;
  JSObject *as_object() const {
    return val.as_object;
  }
  JSObject *as_object_or_null() const {
    return is_object() ? val.as_object : nullptr;
  }

  double as_f64() const {
    switch (tag) {
      case NUM_FLOAT:
        return val.as_f64;
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
    if (tag == STRING) return val.as_prim_string->str.empty();
    return false;
  }

  bool bool_value() const {
    return !is_falsy();
  }

  bool is(JSValueTag val_tag) const {
    return tag == val_tag;
  }

  // simply an alternative for the assignment operator =.
  void assign(const JSValue& rhs) {
    *this = rhs;
  }

  /// `description()` is for internal debugging.
  std::string description() const;
  /// `to_string(NjsVM&)` is for console.log.
  std::string to_string(NjsVM& vm) const;
  /// `to_json(u16string&, NjsVM&)` is for JSON.stringify.
  void to_json(u16string& output, NjsVM& vm) const;

  union {
    double as_f64;
    uint32_t as_atom;
    uint32_t as_symbol;
    uint32_t as_u32;
    int32_t as_i32;
    bool as_bool;

    JSValue *as_JSValue;
    GCObject *as_GCObject;

    PrimitiveString *as_prim_string;
    JSHeapValue *as_heap_val;

    JSObject *as_object;
    JSArray *as_array;
    JSString *as_string;
    JSFunction *as_func;
  } val;

  JSValueTag tag;
  u32 flag_bits {0};
};

inline JSValue& JSValue::deref() const {
  assert(tag == VALUE_HANDLE);
  return *val.as_JSValue;
}

inline const JSValue undefined{JSValue::UNDEFINED};

} // namespace njs

#endif // NJS_JSVALUE_H