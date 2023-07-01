#include "JSValue.h"

#include "njs/basic_types/JSObject.h"
#include "njs/gc/GCObject.h"

namespace njs {

GCObject *JSValue::as_GCObject() {
  assert(tag == OBJECT);
  return static_cast<GCObject *>(val.as_object);
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