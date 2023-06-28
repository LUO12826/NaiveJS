#ifndef NJS_GCOBJECT_H
#define NJS_GCOBJECT_H

#include <cstdint>

#include "njs/include/robin_hood.h"

namespace njs {

class GCHeap;

using robin_hood::unordered_map;
using u32 = uint32_t;

enum ObjectClass {
  CLS_OBJECT = 1,
  CLS_ARRAY,
  CLS_ERROR,
  CLS_DATE,
  CLS_FUNCTION,
};

class GCObject {
 public:
  GCObject(ObjectClass type) : type(type) {}
  virtual ~GCObject() {}

  GCObject(const GCObject& obj) = delete;
  GCObject(GCObject&& obj) = delete;

  virtual void gc_scan_children(GCHeap &heap) = 0;

  u32 size;
  GCObject *forward_ptr{nullptr};
  ObjectClass type;
};

} // namespace njs

#endif // NJS_GCOBJECT_H