#ifndef NJS_JS_HEAP_VALUE_H
#define NJS_JS_HEAP_VALUE_H

#include "njs//gc/GCObject.h"
#include "JSValue.h"

namespace njs {

struct JSHeapValue: public GCObject {
  explicit JSHeapValue(JSValue val): wrapped_val(val) {}

  bool gc_scan_children(njs::GCHeap &heap) override {
    if (wrapped_val.needs_gc()) {
      return heap.gc_visit_object2(wrapped_val, wrapped_val.as_GCObject);
    }
    return false;
  }

  void gc_mark_children() override {
    if (wrapped_val.needs_gc()) {
      gc_mark_object(wrapped_val.as_GCObject);
    }
  }

  bool gc_has_young_child(njs::GCObject *oldgen_start) override {
    return wrapped_val.needs_gc() && wrapped_val.as_GCObject < oldgen_start;
  }

  std::string description() override {
    return "HeapValue: " + wrapped_val.description();
  }

  JSValue wrapped_val;
};

}

#endif //NJS_JS_HEAP_VALUE_H
