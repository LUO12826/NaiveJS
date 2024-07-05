#ifndef NJS_GC_HEAP_H
#define NJS_GC_HEAP_H

#include <vector>
#include <deque>
#include <array>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <condition_variable>

#include "GCObject.h"
#include "njs/utils/helper.h"
#include "njs/global_var.h"

namespace njs {

using std::string_view;
using std::u16string_view;
using std::vector;
using std::array;
using std::deque;
using u32 = uint32_t;

class NjsVM;
struct JSValue;
struct PrimitiveString;

struct GCStats {
  size_t newgen_last_gc_object_cnt {0};
  size_t newgen_object_cnt {0};
  size_t newgen_last_gc_usage {0};
  size_t newgen_gc_count {0};

  size_t oldgen_object_cnt {0};
  size_t oldgen_usage {0};

  size_t total_time {0};
  size_t copy_time {0};
  size_t dealloc_time {0};

  void print() {
    std::cout << "GC trigger count: " << newgen_gc_count << "\n";
    std::cout << "GC total time: " << total_time / 1000 << " ms\n";
    std::cout << "GC copy time: " << copy_time / 1000 << " ms\n";
    std::cout << "GC dealloc time: " << dealloc_time / 1000 << " ms\n";

    std::cout << "newgen last gc usage: " << memory_usage_readable(newgen_last_gc_usage) << "\n";
    std::cout << "newgen last gc object count: " << newgen_last_gc_object_cnt << "\n";

    std::cout << "oldgen usage: " << memory_usage_readable(oldgen_usage) << "\n";
    std::cout << "oldgen object count: " << oldgen_object_cnt << "\n";
  }
};

class GCHeap {

using byte = int8_t;
constexpr static int AGE_MAX = 1;
constexpr static double newgen_size_ratio = 0.4;
constexpr static double survivor_size_ratio = 0.2;
constexpr static double oldgen_size_ratio = 1 - newgen_size_ratio - 2 * survivor_size_ratio;
constexpr static double newgen_gc_threshold_ratio = 0.36;

 public:
  GCHeap(size_t size_mb, NjsVM& vm);
  ~GCHeap();

  /// @brief Create a new object on heap.
  template <typename T, typename... Args>
  T* new_object(Args &&...args) {
    // allocate memory
    GCObject *meta = newgen_alloc(sizeof(T));
    // initialize
    T *object = new (meta) T(std::forward<Args>(args)...);
    meta->size = sizeof(T);

    stats.newgen_object_cnt += 1;
    return object;
  }

  PrimitiveString* new_prim_string_ref(u16string_view str);
  PrimitiveString* new_prim_string(const char16_t *str, size_t length);
  PrimitiveString* new_prim_string(size_t length);

  void gc();
  void gc_if_needed();
  void pause_gc() {gc_pause_counter += 1; }
  void resume_gc() {gc_pause_counter -= 1; }

  // When garbage collection is performed, this method is called to copy an object
  // to a new memory area and have the pointer in JSValue, which is the handle,
  // point to the new address. The `copy_object` method copies the child object recursively.
  // This method will be called not only in this class, but also in the `gc_scan_children` method
  // of the GCObject subclasses.
  bool gc_visit_object(JSValue &handle);

  void write_barrier(GCObject *obj, JSValue const& field);
  bool object_in_newgen(GCObject *obj) { return obj < reinterpret_cast<GCObject *>(oldgen_start); }

  GCStats stats;
 private:
  static void gc_message(string_view msg);

  PrimitiveString* new_prim_string_impl(size_t length);

  // Allocate memory for a new object.
  GCObject* newgen_alloc(size_t size_byte);
  // Copy a single object. Recursively copy its child objects.
  GCObject* copy_object(GCObject *obj);

  void gather_roots();
  void minor_gc_task();
  GCObject* promote(GCObject *obj);
  void newgen_copy_alive();
  static void newgen_dealloc_dead(byte *start, byte *end);
  void newgen_dealloc_dead_with_progress(byte *start, byte *end);

  GCObject* oldgen_alloc(size_t size);
  void major_gc();
  void mark_phase();
  void sweep_phase();
  static void oldgen_dealloc_dead(byte *start, byte *end);

  static void check_fwd_pointer(byte *start, byte *end);

  NjsVM& vm;
  vector<JSValue *> roots;
  vector<JSValue *> const_roots;

  size_t heap_size;
  byte *storage;

  byte *newgen_start;
  byte *survivor1_start;
  byte *survivor2_start;
  byte *oldgen_start;
  byte *oldgen_end;

  // Starting address of the next new object
  byte *alloc_point;
  byte *oldgen_alloc_point;
  byte *survivor_alloc_point;

  byte *survivor_from_start;
  byte *survivor_to_start;

  std::atomic<byte *> dealloc_progress;
  byte *newgen_gc_threshold;

  bool gc_requested {false};
  std::atomic<bool> gc_running {false};
  bool gc_start {false};
  bool copy_done {false};
  bool stop {false};

  std::condition_variable gc_cond_var;
  std::mutex cond_mutex;

  array<deque<GCObject *>, 8> free_list;
  vector<GCObject *> record_set;

  u32 gc_pause_counter {0};

  // must put this at the very end to make sure every thing is initialized.
  std::thread gc_thread;
};

} // namespace njs

#endif // NJS_GC_HEAP_H