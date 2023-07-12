#include "JSValue.h"

#include <sstream>
#include <iomanip>
#include <cmath>

#include "njs/basic_types/JSObject.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/common/enum_strings.h"

namespace njs {

std::string addressToHex(const void* address) {
  std::ostringstream stream;
  stream << "0x" << std::hex << std::setfill('0') << std::setw(sizeof(void*) * 2)
         << reinterpret_cast<uintptr_t>(address);
  return stream.str();
}

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

JSValue& JSValue::deref() const {
  assert(tag == JS_VALUE_REF || tag == HEAP_VAL_REF);
  if (tag == JS_VALUE_REF) return *val.as_js_value;
  return val.as_heap_val->wrapped_val;
}

void JSValue::move_to_heap() {
  auto *heap_val = new JSHeapValue();
  heap_val->retain();
  heap_val->wrapped_val = *this;
  this->val.as_heap_val = heap_val;
  this->tag = HEAP_VAL_REF;
}

std::string JSValue::description() const {

  std::ostringstream stream;

  stream << "JSValue tag: " << js_value_tag_names[tag] << ", ";
  if (tag == BOOLEAN) stream << "val: " << val.as_bool;
  else if (tag == NUM_FLOAT) stream << "val: " << val.as_float64;
  else if (tag == NUM_INT) stream << "val: " << val.as_int;
  else if (tag == STACK_FRAME_META1) {
    stream << "function named: " << to_utf8_string(val.as_function->name)
           << " @" << std::hex << val.as_function;
  }
  else if (tag == STRING) {
    stream << "val: " << to_utf8_string(val.as_primitive_string->str);
  }

  return stream.str();
}

std::string JSValue::to_string() const {
  std::ostringstream stream;

  if (tag == BOOLEAN) stream << val.as_bool;
  else if (tag == NUM_FLOAT) stream << val.as_float64;
  else if (tag == NUM_INT) stream << val.as_int;
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
      val.as_rc_object->release();
    }
    // copy
    val.as_int = rhs.val.as_int;
    flag_bits = rhs.flag_bits;
    tag = rhs.tag;
    // if rhs is an RC object, we are going to retain the new object
    if (is_RCObject()) {
      val.as_rc_object->retain();
    }
}

JSValue JSValue::undefined = JSValue(JSValue::UNDEFINED);
JSValue JSValue::null = JSValue(JSValue::JS_NULL);


}