#include "JSValue.h"

#include <sstream>

#include "njs/basic_types/JSObject.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/common/enum_strings.h"
#include "njs/utils/helper.h"

namespace njs {

GCObject *JSValue::as_GCObject() const {
  assert(tag == OBJECT || tag == FUNCTION || tag == ARRAY || tag == STACK_FRAME_META1);
  return static_cast<GCObject *>(val.as_object);
}

JSValue& JSValue::deref() const {
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

  if (tag == BOOLEAN) stream << to_utf8_string(val.as_bool);
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

void JSValue::to_json(u16string& output, NjsVM& vm) const {
  char16_t num_buf[40];

  switch (tag) {
    case NUM_FLOAT: {
      int len = print_double_u16string(val.as_float64, num_buf);
      u16string_view sv(num_buf, len);
      output += sv;
      break;
    }
    case BOOLEAN:
      output += val.as_bool ? u"true" : u"false";
      break;
    case JS_NULL:
      output += u"null";
      break;
    case STRING:
      output += u'"';
      output += val.as_primitive_string->str;
      output += u'"';
      break;
    case ARRAY:
      val.as_array->to_json(output, vm);
      break;
    default:
      if (is_object()) {
        val.as_object->to_json(output, vm);
      }
      else {
        assert(false);
      }
  }
}

JSValue JSValue::undefined = JSValue(JSValue::UNDEFINED);
JSValue JSValue::null = JSValue(JSValue::JS_NULL);


}