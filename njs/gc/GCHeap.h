#ifndef NJS_GCHEAP_H
#define NJS_GCHEAP_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#include "GCObject.h"
#include "njs/basic_types/JSValue.h"

namespace njs {

class GCHeap;
struct JSValue;

struct GCVisitor {
  GCVisitor(GCHeap &heap) : heap(heap) {}

  void do_visit(JSValue &handle, GCObject *obj);

  GCHeap &heap;
};

class GCHeap {

  using byte = int8_t;
  friend struct GCVisitor;

 public:
  GCHeap(size_t size_mb)
      : heap_size(size_mb * 1024 * 1024), storage((byte *)malloc(size_mb * 1024 * 1024)),
        from_start(storage), to_start(storage + heap_size / 2), alloc_point(storage), visitor(*this) {}

  /// @brief Create a new object on heap.
  template <typename T, typename... Args>
  T *new_object(Args &&...args) {
    // allocate memory
    void *ptr = allocate(sizeof(T));
    // initialize
    T *object = new (ptr) T(std::forward<Args>(args)...);
    // set the metadata
    GCObject *metadata = reinterpret_cast<GCObject *>(ptr);
    metadata->size = sizeof(T);

    return object;
  }

 private:
  std::vector<JSValue *> gather_roots() { return {}; }

  // Copying GC
  void copy_alive() {
    alloc_point = to_start;
    std::vector<JSValue *> roots = gather_roots();

    for (JSValue *root : roots) {
      auto *obj = root->as_GCObject();
      visitor.do_visit(*root, obj);
    }

    std::swap(from_start, to_start);
  }

  // Copy a single object. Recursively copy its child objects.
  GCObject *copy_object(GCObject *obj) {
    if (obj->forward_ptr == nullptr) {
      memcpy(alloc_point, (void *)obj, obj->size);
      obj->forward_ptr = (GCObject *)alloc_point;
      alloc_point += obj->size;

      obj->gc_scan_children(visitor);
    }
    return obj->forward_ptr;
  }

  inline bool lacking_free_memory(size_t size_byte) {
    return alloc_point + size_byte > from_start + heap_size / 2;
  }

  // Allocate memory for a new object.
  void *allocate(size_t size_byte) {
    if (lacking_free_memory(size_byte)) {
      copy_alive();
      if (lacking_free_memory(size_byte)) {
        // allocation fail
      }
    }

    void *start_addr = alloc_point;
    alloc_point += size_byte;
    return start_addr;
  }

  size_t heap_size;
  // heap data
  byte *storage;
  // The start address of the first part
  byte *from_start;
  // The start address of the second part
  byte *to_start;
  // Starting address of the next new object
  byte *alloc_point;

  GCVisitor visitor;
};

} // namespace njs

#endif // NJS_GCHEAP_H