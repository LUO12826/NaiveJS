#include "JSArray.h"

#include <string>
#include "njs/gc/GCHeap.h"

namespace njs {

void JSArray::gc_scan_children(GCHeap& heap) {
  JSObject::gc_scan_children(heap);
  for (auto& val : dense_array) {
    if (val.needs_gc()) {
      heap.gc_visit_object(val, val.as_GCObject());
    }
  }
}

std::string JSArray::description() {
  std::string desc = "Array[ ";

  for (auto& val : dense_array) {
    desc += val.to_string();
    desc += ", ";
  }
  desc += "]";
  desc += " props: ";
  desc += JSObject::description();

  return desc;
}

void JSArray::to_json(u16string& output, NjsVM& vm) const {
  output += u"[";

  bool first = true;
  
  for (auto& val : dense_array) {
    if (first) first = false;
    else output += u',';

    if (val.tag_is(JSValue::JS_NULL) || val.tag_is(JSValue::UNDEFINED)) {
      output += u"null";
    }
    else {
      val.to_json(output, vm);
    }
  }

  output += u"]";
}

JSValue JSArray::access_element(u32 index, bool create_ref) {
  if (create_ref) {
    if (index >= dense_array.size()) dense_array.resize(long(index * 1.2));
    return JSValue(&dense_array[index]);
  }
  else {
    if (index >= dense_array.size()) return JSValue::undefined;
    return dense_array[index];
  }
}

}