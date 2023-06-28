#ifndef NJS_RCOBJECT_H
#define NJS_RCOBJECT_H

#include <cstdint>

namespace njs {

using u32 = uint32_t;

class RCObject {
 public:
  RCObject() {}
  virtual ~RCObject() {}

  RCObject(const RCObject& obj) = delete;
  RCObject(RCObject&& obj) = delete;

  void retain() { ref_count += 1; }

  void release() {
    ref_count -= 1;
    if (ref_count == 0) delete this;
  }

 private:
  u32 ref_count;
};

} // namespace njs

#endif // NJS_RCOBJECT_H