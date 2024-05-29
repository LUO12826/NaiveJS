#include "JSArray.h"

#include <string>
#include "njs/gc/GCHeap.h"
#include "njs/vm/NjsVM.h"

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
    desc += val.description();
    desc += ", ";
  }
  desc += "]";
  desc += " props: ";
  desc += JSObject::description();

  return desc;
}

std::string JSArray::to_string(NjsVM& vm) const {
  std::string output = "[ ";

  for (auto& val : dense_array) {
    if (val.is_uninited()) {
      output += "<empty item>";
    } else {
      output += val.to_string(vm);
    }
    output += ", ";
  }

  if (!storage.empty()) {
    for (auto& [key, prop] : storage) {
      if (not prop.flag.enumerable) continue;

      if (key.type == JSValue::JS_ATOM) {
        u16string escaped = to_escaped_u16string(vm.atom_to_str(key.atom));
        output += to_u8string(escaped);
      }

      output += ": ";
      if (prop.data.value.is(JSValue::STRING)) output += '"';
      output += prop.data.value.to_string(vm);
      if (prop.data.value.is(JSValue::STRING)) output += '"';
      output += ", ";
    }
  }

  output += "]";
  return output;
}

void JSArray::to_json(u16string& output, NjsVM& vm) const {
  output += u"[";

  bool first = true;
  
  for (auto& val : dense_array) {
    if (first) first = false;
    else output += u',';

    if (val.is(JSValue::JS_NULL) || val.is(JSValue::UNDEFINED)) {
      output += u"null";
    }
    else {
      val.to_json(output, vm);
    }
  }

  output += u"]";
}

}