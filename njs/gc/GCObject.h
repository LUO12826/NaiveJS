#ifndef NJS_GCOBJECT_H
#define NJS_GCOBJECT_H

#include <cstdint>

namespace njs {

class GCHeap;
using u32 = uint32_t;

class GCObject {
friend class GCHeap;

 public:
  GCObject() = default;
  virtual ~GCObject() = default;

  GCObject(const GCObject& obj) = delete;
  GCObject(GCObject&& obj) = delete;

  static void gc_mark_object(GCObject *obj) {
    if (not obj->gc_visited) {
      obj->gc_visited = true;
      obj->gc_mark_children();
    }
  }

  virtual bool gc_scan_children(GCHeap &heap) { return false; }
  virtual void gc_mark_children() {}
  virtual bool gc_has_young_child(GCObject *oldgen_start) { return false; }
  virtual std::string description() = 0;

  void set_visited() {
    gc_visited = true;
  }

  void ref_count_inc() {
    ref_count += (ref_count < 15);
  }

  uint8_t get_ref_count() {
    return ref_count;
  }

 private:
  u32 size;
  uint8_t gc_age : 4 {0};
  uint8_t ref_count : 4 {0};
  bool gc_visited;
  bool gc_free;
  bool gc_remembered;
  GCObject *forward_ptr {nullptr};
};

} // namespace njs

#endif // NJS_GCOBJECT_H