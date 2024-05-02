#ifndef NJS_RCOBJECT_H
#define NJS_RCOBJECT_H

#include <cstdint>
#include <cstdio>
#include <cassert>
#include "njs/global_var.h"

namespace njs {

using u32 = uint32_t;

/// Objects that manage its memory using reference counting.
class RCObject {
 public:
  RCObject() = default;
  virtual ~RCObject() = default;

  RCObject(const RCObject& obj) = delete;
  RCObject(RCObject&& obj) = delete;

  virtual RCObject *copy() {
    return new RCObject();
  }

  void retain() { ref_count += 1; }

  void release() {
    assert(ref_count != 0);
    ref_count -= 1;
    if (ref_count == 0) {
      if (Global::show_gc_statistics) printf("RC remove an RCObject\n");
      delete this;
    }
  }

  void mark_as_temp() {
    ref_count -= 1;
    assert(ref_count >= 0);
  }

  void delete_temp_object() {
    assert(ref_count == 0);
    if (Global::show_gc_statistics) printf("RC remove an temporary RCObject\n");
    delete this;
  }

  u32 get_ref_count() const {
    return ref_count;
  }

 private:
  u32 ref_count {0};
};

} // namespace njs

#endif // NJS_RCOBJECT_H