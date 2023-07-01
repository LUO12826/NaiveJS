#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"

namespace njs {

void JSObject::gc_scan_children(GCHeap& heap) {
  for (auto& kv_pair: storage) {
    JSValue& val = kv_pair.second;

    if (val.needs_gc()) {
      heap.gc_visit_object(val, val.as_GCObject());
    }
  }
}

}