#include "JSValue.h"

#include <sstream>
#include <iomanip>
#include <cmath>

#include "njs/basic_types/JSObject.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/common/enum_strings.h"

namespace njs {

GCObject *JSValue::as_GCObject() const {
  assert(tag == OBJECT || tag == FUNCTION || tag == ARRAY || tag == STACK_FRAME_META1);
  return static_cast<GCObject *>(val.as_object);
}

bool JSValue::is_falsy() const {
  if (tag == BOOLEAN) return !val.as_bool;
  if (tag == JS_NULL || tag == UNDEFINED) return true;
  if (tag == NUM_FLOAT) return val.as_float64 == 0 || std::isnan(val.as_float64);
  if (tag == STRING) return val.as_primitive_string->str.empty();
  return false;
}

bool JSValue::tag_is(JSValueTag val_tag) const {
  return tag == val_tag;
}

JSValue& JSValue::deref() {
  assert(tag == VALUE_HANDLE || tag == HEAP_VAL);
  if (tag == VALUE_HANDLE) return *val.as_JSValue;
  else return val.as_heap_val->wrapped_val;
}

JSValue& JSValue::deref_if_needed() {
  if (tag == VALUE_HANDLE) return *val.as_JSValue;
  else if (tag == HEAP_VAL) return val.as_heap_val->wrapped_val;
  else return *this;
}

void JSValue::move_to_heap() {
  auto *heap_val = new JSHeapValue(*this);
  heap_val->retain();
  this->val.as_heap_val = heap_val;
  this->tag = HEAP_VAL;
}

std::string JSValue::description() const {

  std::ostringstream stream;

  stream << "JSValue(tag: " << js_value_tag_names[tag];
  if (tag == BOOLEAN) stream << ", val: " << val.as_bool;
  else if (tag == NUM_FLOAT) stream << ", val: " << val.as_float64;
  else if (tag == NUM_INT) stream << ", val: " << val.as_int64;
  else if (tag == STACK_FRAME_META1) {
    stream << ", function named: " << to_utf8_string(val.as_function->name)
           << " @" << std::hex << val.as_function;
  }
  else if (tag == STRING) {
    stream << ", val: " << to_utf8_string(val.as_primitive_string->str);
  }
  stream << ")";

  return stream.str();
}

std::string JSValue::to_string() const {
  std::ostringstream stream;

  if (tag == BOOLEAN) stream << val.as_bool;
  else if (tag == NUM_FLOAT) stream << val.as_float64;
  else if (tag == NUM_INT) stream << val.as_int64;
  else if (tag == UNDEFINED) stream << "undefined";
  else if (tag == JS_NULL) stream << "null";
  else if (tag == STRING) stream << to_utf8_string(val.as_primitive_string->str);
  else if (tag == STACK_FRAME_META1 || is_object()) {
    stream << as_GCObject()->description();
  }

  return stream.str();
}

void JSValue::assign(JSValue& rhs) {
    assert(tag != STACK_FRAME_META1 && tag != STACK_FRAME_META2);
    assert(rhs.tag != STACK_FRAME_META1 && rhs.tag != STACK_FRAME_META2);

    // if this is an RC object, we are going to release the old object
    if (is_RCObject()) {
      val.as_RCObject->release();
    }
    // copy
    val.as_int64 = rhs.val.as_int64;
    flag_bits = rhs.flag_bits;
    tag = rhs.tag;
    // if rhs is an RC object, we are going to retain the new object
    if (is_RCObject()) {
      val.as_RCObject->retain();
    }
}

JSValue JSValue::undefined = JSValue(JSValue::UNDEFINED);
JSValue JSValue::null = JSValue(JSValue::JS_NULL);


}