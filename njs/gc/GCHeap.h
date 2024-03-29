#ifndef NJS_GCHEAP_H
#define NJS_GCHEAP_H

#include <utility>
#include <vector>
#include <iostream>

#include "GCObject.h"
#include "njs/global_var.h"
#include "njs/basic_types/JSValue.h"

namespace njs {

class NjsVM;

class GCHeap {

  using byte = int8_t;

 public:

  GCHeap(size_t size_mb, NjsVM& vm)
      : heap_size(size_mb * 1024 * 1024), storage((byte *)malloc(size_mb * 1024 * 1024)),
        from_start(storage), to_start(storage + heap_size / 2), alloc_point(storage),
        vm(vm) {

    if (Global::show_gc_statistics) {
      std::cout << "GCHeap init, from_start == " << (size_t)from_start << '\n';
//      std::cout << "sizeof JSFunction: " << sizeof(JSFunction) << '\n';
//      std::cout << "sizeof JSObject:  " << sizeof(JSObject) << '\n';
//      std::cout << "sizeof JSArray:  " << sizeof(JSArray) << '\n';
    }
  }

  /// @brief Create a new object on heap.
  template <typename T, typename... Args>
  T *new_object(Args &&...args) {
    // allocate memory
    void *ptr = allocate(sizeof(T));
    // initialize
    T *object = new (ptr) T(std::forward<Args>(args)...);
    // set the metadata
    auto *metadata = reinterpret_cast<GCObject *>(ptr);
    metadata->size = sizeof(T);

    return object;
  }

  void gc();

  // When garbage collection is performed, this method is called to copy an object
  // to a new memory area and have the pointer in JSValue, which is the handle,
  // point to the new address. The `copy_object` method copies the child object recursively.
  // This method will be called not only in this class, but also in the `gc_scan_children` method
  // of the GCObject subclasses.
  void gc_visit_object(JSValue &handle, GCObject *obj);

 private:
  std::vector<JSValue *> gather_roots();

  // Copying GC
  void copy_alive();

  void dealloc_dead(byte *start, byte *end);

  // Copy a single object. Recursively copy its child objects.
  GCObject *copy_object(GCObject *obj);

  inline bool lacking_free_memory(size_t size_byte) {
    return alloc_point + size_byte > from_start + heap_size / 2;
  }

  // Allocate memory for a new object.
  void *allocate(size_t size_byte);

  size_t heap_size;
  // heap data
  byte *storage;
  // The start address of the first part
  byte *from_start;
  // The start address of the second part
  byte *to_start;
  // Starting address of the next new object
  byte *alloc_point;

  NjsVM& vm;

};

} // namespace njs

#endif // NJS_GCHEAP_H