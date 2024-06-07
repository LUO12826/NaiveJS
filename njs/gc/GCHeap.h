#ifndef NJS_GC_HEAP_H
#define NJS_GC_HEAP_H

#include <utility>
#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <condition_variable>

#include "GCObject.h"
#include "njs/global_var.h"

namespace njs {

using std::string_view;
using std::vector;
class NjsVM;
struct JSValue;
struct PrimitiveString;

struct GCStats {
  long long gc_count {0};
  long long total_time {0};
  long long copy_time {0};
  long long dealloc_time {0};

  void print() {
    std::cout << "GC trigger count: " << gc_count << "\n";
    std::cout << "GC total time: " << total_time / 1000 << " ms\n";
    std::cout << "GC copy time: " << copy_time / 1000 << " ms\n";
    std::cout << "GC dealloc time: " << dealloc_time / 1000 << " ms\n";
  }
};

class GCHeap {
friend struct PrimitiveString;
using byte = int8_t;

 public:
  GCHeap(size_t size_mb, NjsVM& vm)
      : heap_size(size_mb * 1024 * 1024),
        low_mem_threshold(0.95 * heap_size / 2),
        storage((byte *)malloc(size_mb * 1024 * 1024)),
        from_start(storage),
        to_start(storage + heap_size / 2),
        alloc_point(storage),
        vm(vm),
        gc_thread(&GCHeap::gc_task, this) {

    if (Global::show_gc_statistics) {
      std::cout << "GCHeap init, from_start == " << (size_t)from_start << '\n';
    }
  }

  ~GCHeap() {
    gc_running.wait(true);
    {
      std::lock_guard<std::mutex> lock(cond_mutex);
      stop = true;
    }
    gc_cond_var.notify_one();
    gc_thread.join();

    dealloc_dead(from_start, alloc_point);

    free(storage);
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
    object_cnt += 1;
    return object;
  }

  PrimitiveString* new_prim_atom_string(u32 atom);
  PrimitiveString* new_prim_string(const char16_t *str, size_t length);
  PrimitiveString* new_prim_string(size_t length);

  void gc();
  void gc_if_needed();
  size_t get_heap_usage() {
    return alloc_point - from_start;
  }
  size_t get_object_count() {
    return object_cnt;
  }

  // When garbage collection is performed, this method is called to copy an object
  // to a new memory area and have the pointer in JSValue, which is the handle,
  // point to the new address. The `copy_object` method copies the child object recursively.
  // This method will be called not only in this class, but also in the `gc_scan_children` method
  // of the GCObject subclasses.
  void gc_visit_object(JSValue &handle, GCObject *obj);

  GCStats stats;
 private:
  static void gc_message(string_view msg);
  PrimitiveString* new_prim_string_impl(size_t length);
  vector<JSValue *> gather_roots();

  void gc_task();
  void copy_alive();
  void dealloc_dead(byte *start, byte *end);
  void check_fwd_pointer();

  // Copy a single object. Recursively copy its child objects.
  GCObject *copy_object(GCObject *obj);

  // Allocate memory for a new object.
  void *allocate(size_t size_byte);

  size_t heap_size;
  size_t low_mem_threshold;
  // heap data
  byte *storage;
  // The start address of the first part
  byte *from_start;
  // The start address of the second part
  byte *to_start;
  // Starting address of the next new object
  byte *alloc_point;

  size_t object_cnt {0};
  size_t last_gc_object_cnt {0};
  size_t last_gc_heap_usage {0};
  size_t last_gc_root_count {50};
  size_t gc_threshold {15000};

  bool gc_requested {false};

  NjsVM& vm;

  std::thread gc_thread;

  std::atomic<bool> gc_running {false};
  bool gc_start {false};
  bool copy_done {false};
  bool stop {false};

  std::condition_variable gc_cond_var;
  std::mutex cond_mutex;

};

} // namespace njs

#endif // NJS_GC_HEAP_H