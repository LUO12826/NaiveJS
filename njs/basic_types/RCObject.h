#ifndef NJS_RCOBJECT_H
#define NJS_RCOBJECT_H

#include <cstdint>

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

  void retain();
  void release();
  void mark_as_temp();
  void delete_temp_object();
  u32 get_ref_count() const;

 private:
  u32 ref_count {0};
};

} // namespace njs

#endif // NJS_RCOBJECT_H