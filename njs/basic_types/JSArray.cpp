#include "JSArray.h"

#include <sstream>
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
  std::ostringstream stream;

  stream << "Array[ ";
  for (auto& val : dense_array) {
    stream << val.to_string() << ", ";
  }
  stream << "]";
  stream << " props: " << JSObject::description();

  return stream.str();
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