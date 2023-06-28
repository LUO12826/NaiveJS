#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"

namespace njs {

void JSObject::gc_scan_children(GCHeap& heap) {
  heap.gc_visit_object(value1, value1.as_GCObject());
  heap.gc_visit_object(value2, value2.as_GCObject());
}

}