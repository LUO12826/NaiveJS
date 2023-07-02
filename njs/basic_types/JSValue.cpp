#include "JSValue.h"

#include "njs/basic_types/JSObject.h"
#include "njs/gc/GCObject.h"

namespace njs {

GCObject *JSValue::as_GCObject() const {
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

const char *js_value_tag_names[22] = {
  "UNDEFINED",
  "JS_ATOM",
  "JS_NULL",
  "BOOLEAN",
  "NUMBER_INT",
  "NUMBER_FLOAT",

  "STRING",
  "SYMBOL",

  "HEAP_VAL_REF",
  "STRING_REF",
  "SYMBOL_REF",

  "STACK_FRAME_META1",
  "STACK_FRAME_META2",
  "OTHER",

  "NEED_GC_BEGIN",

  "BOOLEAN_OBJ",
  "NUMBER_OBJ",
  "STRING_OBJ",
  "OBJECT",
  "ARRAY",
  "FUNCTION",

  "NEED_GC_END"
};

}