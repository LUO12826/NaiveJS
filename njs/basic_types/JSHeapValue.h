#ifndef NJS_JS_HEAP_VALUE_H
#define NJS_JS_HEAP_VALUE_H

#include "JSValue.h"
#include "njs/vm/NjsVM.h"
#include "njs/utils/macros.h"

namespace njs {

struct JSHeapValue: public GCObject {
  explicit JSHeapValue(NjsVM& vm, JSValue val): wrapped_val(val) {
    gc_write_barrier(val);
  }

  bool gc_scan_children(njs::GCHeap &heap) override {
    if (wrapped_val.needs_gc()) {
      return heap.gc_visit_object(wrapped_val);
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
