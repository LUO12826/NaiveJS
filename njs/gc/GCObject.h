#ifndef NJS_GCOBJECT_H
#define NJS_GCOBJECT_H

#include <cstdint>

#include "njs/include/robin_hood.h"

namespace njs {

class GCHeap;

using robin_hood::unordered_map;
using u32 = uint32_t;

class GCObject {
 public:
  GCObject(u32 size): size(size) {}
  virtual ~GCObject() = default;

  GCObject(const GCObject& obj) = delete;
  GCObject(GCObject&& obj) = delete;

  virtual void gc_scan_children(GCHeap &heap) = 0;
  virtual std::string description() = 0;

  u32 size;
  GCObject *forward_ptr{nullptr};
};

} // namespace njs

#endif // NJS_GCOBJECT_H