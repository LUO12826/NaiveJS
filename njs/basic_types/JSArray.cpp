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

std::string JSArray::to_string(NjsVM& vm) {
  std::string output = "[ ";

  for (auto& val : dense_array) {
    output += val.to_string(vm);
    output += ", ";
  }

//  if (!storage.empty()) {
//    for (auto& [key, val] : storage) {
//      if (key.key_type == JSObjectKey::KEY_ATOM) {
//        output += to_u8string(vm.str_pool.get_string(key.key.atom));
//      }
//      else assert(false);
//
//      output += ": ";
//      output += val.to_string(vm);
//      output += ", ";
//    }
//  }

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