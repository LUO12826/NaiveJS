#include "JSValue.h"

#include <sstream>

#include "njs/basic_types/JSObject.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSFunction.h"
#include "JSHeapValue.h"
#include "njs/common/enum_strings.h"
#include "njs/utils/helper.h"

namespace njs {

GCObject *JSValue::as_GCObject() const {
  assert(tag == OBJECT || tag == FUNCTION || tag == ARRAY || tag == STACK_FRAME_META1);
  return static_cast<GCObject *>(val.as_object);
}

void JSValue::move_to_heap() {
  auto *heap_val = new JSHeapValue(*this);
  heap_val->retain();
  this->val.as_heap_val = heap_val;
  this->tag = HEAP_VAL;
}

JSValue& JSValue::deref_heap() const {
  assert(tag == HEAP_VAL);
  return val.as_heap_val->wrapped_val;
}

std::string JSValue::description() const {

  std::ostringstream stream;

  stream << "JSValue(tag: " << js_value_tag_names[tag];
  if (tag == BOOLEAN) stream << ", value: " << val.as_bool;
  else if (tag == NUM_FLOAT) stream << ", value: " << val.as_f64;
  else if (tag == NUM_INT64) stream << ", value: " << val.as_i64;
  else if (is_object()) {
    stream << ", obj: " << as_GCObject()->description();
  }
  else if (tag == STACK_FRAME_META1) {
    stream << ", function named: " << to_u8string(val.as_function->name)
           << " @" << std::hex << val.as_function;
  }
  else if (tag == STRING) {
    stream << ", value: " << to_u8string(val.as_primitive_string->str);
  }
  stream << ")";

  return stream.str();
}

std::string JSValue::to_string(NjsVM& vm) const {
  std::string output;
  switch (tag) {
    case UNDEFINED: output += "undefined"; break;
    case JS_NULL: output += "null"; break;
    case JS_ATOM:
      output += "Atom(" + std::to_string(val.as_i64) + ')';
      break;
    case BOOLEAN:
      output += val.as_bool ? "true" : "false";
      break;
    case NUM_INT64: output += std::to_string(val.as_i64);
      break;
    case NUM_FLOAT: {
      char num_buf[40];
      int len = print_double_string(val.as_f64, num_buf);
      output += std::string_view(num_buf, len);
      break;
    }
    case HEAP_VAL:
      output += deref_heap().to_string(vm);
      break;
    case VALUE_HANDLE:
      output += deref().to_string(vm);
      break;
    case STRING: {
//      u16string escaped = to_escaped_u16string(val.as_primitive_string->str);
//      output += to_u8string(escaped);
      output += to_u8string(val.as_primitive_string->str);
      break;
    }
    case SYMBOL: break;
    case STACK_FRAME_META1:
      output += val.as_function->to_string(vm);
      break;
    default:
      if (is_object()) {
        output += val.as_object->to_string(vm);
      }
  }

  return output;
}

void JSValue::to_json(u16string& output, NjsVM& vm) const {

  switch (tag) {
    case NUM_FLOAT: {
      char16_t num_buf[40];
      int len = print_double_u16string(val.as_f64, num_buf);
      output += u16string_view(num_buf, len);
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
      output += to_escaped_u16string(val.as_primitive_string->str);
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