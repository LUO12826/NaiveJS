#ifndef NJS_GCOBJECT_H
#define NJS_GCOBJECT_H

#include <cstdint>

#include "njs/include/robin_hood.h"

namespace njs {

class GCHeap;
using u32 = uint32_t;

class GCObject {
 public:
  GCObject() = default;
  explicit GCObject(u32 size): size(size) {}
  virtual ~GCObject() = default;

  GCObject(const GCObject& obj) = delete;
  GCObject(GCObject&& obj) = delete;

  virtual void gc_scan_children(GCHeap &heap) = 0;
  virtual std::string description() = 0;

  void set_nocopy() {
    forward_ptr = (GCObject *)NO_COPY;
  }

  bool need_copy() {
    return forward_ptr != (GCObject *)NO_COPY;
  }

  static constexpr size_t NO_COPY {SIZE_T_MAX};

  u32 size;
  GCObject *forward_ptr{nullptr};
};

} // namespace njs

#endif // NJS_GCOBJECT_H