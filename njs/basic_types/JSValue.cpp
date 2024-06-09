#include "JSValue.h"

#include <sstream>

#include "njs/basic_types/JSObject.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSFunction.h"
#include "JSHeapValue.h"
#include "njs/common/enum_strings.h"
#include "njs/utils/helper.h"
#include "njs/vm/NjsVM.h"

namespace njs {

void JSValue::move_to_heap(NjsVM& vm) {
  auto *heap_val = vm.heap.new_object<JSHeapValue>(vm, *this);
  this->as_heap_val = heap_val;
  this->tag = HEAP_VAL;
}

void JSValue::move_to_stack() {
  assert(tag == HEAP_VAL);
  *this = as_heap_val->wrapped_val;
}

JSValue& JSValue::deref_heap() const {
  assert(tag == HEAP_VAL);
  return as_heap_val->wrapped_val;
}

JSValue& JSValue::deref_heap_if_needed() {
  if (tag == HEAP_VAL) [[unlikely]] {
    return as_heap_val->wrapped_val;
  } else {
    return *this;
  }
}


std::string JSValue::description() const {

  std::ostringstream stream;

  stream << "JSValue(tag: " << js_value_tag_names[tag];
  if (tag == BOOLEAN) stream << ", value: " << as_bool;
  else if (tag == NUM_FLOAT) stream << ", value: " << as_f64;
  else if (is_object()) {
    stream << ", obj: " << as_GCObject->description();
  }
  else if (tag == STRING) {
    stream << ", value: " << to_u8string(as_prim_string->view());
  }
  stream << ")";

  return stream.str();
}

std::string JSValue::to_string(NjsVM& vm) const {
  std::string output;
  switch (tag) {
    case UNINIT:
    case UNDEFINED: output += "undefined"; break;
    case JS_NULL: output += "null"; break;
    case JS_ATOM:
      output += "Atom(" + std::to_string(as_atom) + ')';
      break;
    case BOOLEAN:
      output += as_bool ? "true" : "false";
      break;
    case NUM_FLOAT: {
      char num_buf[40];
      int len = print_double_string(as_f64, num_buf);
      output += std::string_view(num_buf, len);
      break;
    }
    case HEAP_VAL:
      output += deref_heap().to_string(vm);
      break;
    case VALUE_HANDLE:
      output += deref().to_string(vm);
      break;
    case STRING:
      output += to_u8string(as_prim_string->view());
      break;
    case SYMBOL:
      output += "Symbol(" + std::to_string(as_symbol) + ')';
      break;
    default:
      if (is_object()) {
        output += as_object->to_string(vm);
      }
  }

  return output;
}

void JSValue::to_json(u16string& output, NjsVM& vm) const {

  switch (tag) {
    case NUM_FLOAT: {
      char16_t num_buf[40];
      int len = print_double_u16string(as_f64, num_buf);
      output += u16string_view(num_buf, len);
      break;
    }
    case BOOLEAN:
      output += as_bool ? u"true" : u"false";
      break;
    case JS_NULL:
      output += u"null";
      break;
    case STRING:
      output += u'"';
      output += to_escaped_u16string(as_prim_string->view());
      output += u'"';
      break;
    case ARRAY:
      as_array->to_json(output, vm);
      break;
    default:
      if (is_object()) {
        as_object->to_json(output, vm);
      } else {
        assert(false);
      }
  }
}

JSValue JSValue::undefined = JSValue(JSValue::UNDEFINED);
JSValue JSValue::uninited = JSValue(JSValue::UNINIT);
JSValue JSValue::null = JSValue(JSValue::JS_NULL);


}