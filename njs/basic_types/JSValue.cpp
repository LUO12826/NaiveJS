#include "JSValue.h"

#include <sstream>
#include <iomanip>

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
  assert(tag == OBJECT || tag == FUNCTION || tag == STACK_FRAME_META1);
  return static_cast<GCObject *>(val.as_object);
}

std::string JSValue::description() const {

  std::ostringstream stream;

  stream << "JSValue tagged: " << js_value_tag_names[tag];
  if (tag == BOOLEAN) stream << ", value: " << val.as_bool;
  else if (tag == NUMBER_FLOAT) stream << ", value: " << val.as_float64;
  else if (tag == NUMBER_INT) stream << ", value: " << val.as_int;
  else if (tag == STACK_FRAME_META1) {
    stream << ", function named: " << to_utf8_string(val.as_function->name)
           << " @" << std::hex << val.as_function;
  }

  return stream.str();
}

std::string JSValue::to_string() const {
  std::ostringstream stream;

  if (tag == BOOLEAN) stream << val.as_bool;
  else if (tag == NUMBER_FLOAT) stream << val.as_float64;
  else if (tag == NUMBER_INT) stream << val.as_int;
  else if (tag == STACK_FRAME_META1 || is_object()) {
    stream << as_GCObject()->description();
  }

  return stream.str();
}

JSValue JSValue::add(JSValue& rhs) {
  if (this->is_int() && rhs.is_float()) {
    return JSValue(this->val.as_int + rhs.val.as_int);
  }
  else if (this->is_float() && rhs.is_float()) {
    return JSValue(this->val.as_float64 + rhs.val.as_float64);
  }
  else {
    assert(false);
  }
}

void JSValue::assign(JSValue& rhs) {
    assert(tag != STACK_FRAME_META1 && tag != STACK_FRAME_META2);
    assert(rhs.tag != STACK_FRAME_META1 && rhs.tag != STACK_FRAME_META2);

    // if this is an RC object, we are going to release the old object,
    // and retain the newly referenced object.
    if (is_RCObject()) {
      val.as_rc_object->release();
      val.as_int = rhs.val.as_int;
      flag_bits = rhs.flag_bits;
      tag = rhs.tag;

      if (is_RCObject()) {
        val.as_rc_object->retain();
      }
    }
    // else, just directly copy the bits.
    else {
      val.as_int = rhs.val.as_int;
      flag_bits = rhs.flag_bits;
      tag = rhs.tag;
    }
}

JSValue::~JSValue() {
  switch (tag) {
    case HEAP_VAL_REF:
      val.as_heap_val->release();
      break;
    case STRING_REF:
      val.as_primitive_string->release();
      break;
    case SYMBOL_REF:
      val.as_symbol->release();
      break;
    default:
      return;
  }
}

JSValue JSValue::undefined = JSValue(JSValue::UNDEFINED);
JSValue JSValue::null = JSValue(JSValue::JS_NULL);


}