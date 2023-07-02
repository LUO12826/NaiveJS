#ifndef NJS_GCOBJECT_H
#define NJS_GCOBJECT_H

#include <cstdint>

#include "njs/include/robin_hood.h"

namespace njs {

class GCHeap;

using robin_hood::unordered_map;
using u32 = uint32_t;

enum class ObjectClass {
  CLS_OBJECT = 1,
  CLS_ARRAY,
  CLS_ERROR,
  CLS_DATE,
  CLS_FUNCTION,
  CLS_CUSTOM
};

class GCObject {
 public:
  explicit GCObject(ObjectClass cls) : obj_class(cls), size(sizeof(GCObject)) {}
  virtual ~GCObject() = default;

  GCObject(const GCObject& obj) = delete;
  GCObject(GCObject&& obj) = delete;

  virtual void gc_scan_children(GCHeap &heap) = 0;

  u32 size;
  GCObject *forward_ptr{nullptr};
  ObjectClass obj_class;
};

} // namespace njs

#endif // NJS_GCOBJECT_H