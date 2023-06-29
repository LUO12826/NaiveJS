#ifndef NJS_JSOBJECT_H
#define NJS_JSOBJECT_H

#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCObject.h"
#include <cstdint>

namespace njs {

class GCHeap;

class JSObject : public GCObject {
 public:
  JSObject() : GCObject(ObjectClass::CLS_OBJECT) {}
  JSObject(ObjectClass cls) : GCObject(cls) {}

  void gc_scan_children(GCHeap& heap) override;

  JSValue value1 {0.0};
  JSValue value2 {0.0};
};

} // namespace njs

#endif // NJS_JSOBJECT_H