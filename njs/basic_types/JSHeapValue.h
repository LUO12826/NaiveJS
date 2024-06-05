#ifndef NJS_JS_HEAP_VALUE_H
#define NJS_JS_HEAP_VALUE_H

#include "njs//gc/GCObject.h"
#include "JSValue.h"

namespace njs {

struct JSHeapValue: public GCObject {
  explicit JSHeapValue(JSValue val): wrapped_val(val) {}

  void gc_scan_children(njs::GCHeap &heap) override {
    if (wrapped_val.needs_gc()) {
      heap.gc_visit_object(wrapped_val, wrapped_val.as_GCObject);
    }
  }

  std::string description() override {
    return "HeapValue: " + wrapped_val.description();
  }

  JSValue wrapped_val;
};

}

#endif //NJS_JS_HEAP_VALUE_H
