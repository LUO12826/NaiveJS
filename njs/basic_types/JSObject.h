#ifndef NJS_JSOBJECT_H
#define NJS_JSOBJECT_H

#include "njs/gc/GCObject.h"
#include <cstdint>

namespace njs {

class JSObject : public GCObject {

  JSObject() : GCObject(ObjectClass::CLS_OBJECT) {}

  void gc_scan_children(GCVisitor visitor) {
    visitor.do_visit(value1, (GCObject *)value1.val.as_ptr);
    visitor.do_visit(value2, (GCObject *)value2.val.as_ptr);
  }

  JSValue value1;
  JSValue value2;
};

} // namespace njs

#endif // NJS_JSOBJECT_H