#ifndef NJS_HEAP_ARRAY_H
#define NJS_HEAP_ARRAY_H

#include "njs/gc/GCObject.h"
#include "njs/gc/GCHeap.h"
#include "njs/common/common_def.h"
#include "njs/utils/macros.h"
#include "njs/basic_types/JSValue.h"

namespace njs {

template <typename T>
struct HeapArray: public GCObject {
friend class GCHeap;

  HeapArray(const HeapArray& other) = delete;
  HeapArray(HeapArray&& other) = delete;
  HeapArray& operator=(HeapArray&& other) = delete;
  HeapArray& operator=(const HeapArray& other) = delete;

  std::string description() override {
    return "HeapArray";
  }

  bool gc_scan_children(GCHeap &heap) override { return false; }
  void gc_mark_children() override {}
  bool gc_has_young_child(GCObject *oldgen_start) override { return false; }

  T& operator[](size_t index) {
    assert(index < len);
    return storage[index];
  }

  T& at(size_t index) {
    assert(index < len);
    return storage[index];
  }

  bool empty() {
    return len == 0;
  }

  u32 size() {
    return len;
  }

 private:
  explicit HeapArray(u32 length, u32 capacity) : len(length), cap(capacity) {}

  u32 len;
  u32 cap;
  T storage[0];
};

template <>
inline std::string HeapArray<JSValue>::description() {
  return "HeapArray<JSValue>";
}

template <>
inline bool HeapArray<JSValue>::gc_scan_children(GCHeap& heap) {
  bool child_young = false;
  for (u32 i = 0; i < len; i++) {
    gc_check_and_visit_object(child_young, storage[i]);
  }
  return child_young;
}

template <>
inline void HeapArray<JSValue>::gc_mark_children() {
  for (u32 i = 0; i < len; i++) {
    gc_check_and_mark_object(storage[i]);
  }
}

template <>
inline bool HeapArray<JSValue>::gc_has_young_child(GCObject *oldgen_start) {
  for (u32 i = 0; i < len; i++) {
    gc_check_object_young(storage[i]);
  }
  return false;
}


} // namespace njs

#endif //NJS_HEAP_ARRAY_H
