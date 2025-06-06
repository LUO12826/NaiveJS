#include <cstring>
#include <iostream>
#include <cstdint>

#include "GCHeap.h"
#include "njs/vm/NjsVM.h"
#include "njs/vm/JSStackFrame.h"
#include "njs/global_var.h"
#include "njs/utils/Timer.h"
#include "njs/basic_types/PrimitiveString.h"
#include "njs/basic_types/HeapArray.h"
#include "njs/common/common_def.h"

namespace njs {

force_inline size_t next_multiple_of_8(size_t num) {
  return (num + 7) & ~7;
}

force_inline int size_to_index(u32 size) {
  // 0 : [0, 64)
  // 1 : [64, 128)
  // 2 : [128, 192)
  // 3 : [192, 256)
  // 4 : [256, 512)
  // 5 : [512, 768)
  // 6 : [768, 4096)
  // 7 : [4096, )
  if (size < 256) [[likely]] {
    return size / 64;
  } else if (size < 768) {
    return (size / 256) + 3;
  } else if (size < 4096) {
    return 6;
  } else {
    return 7;
  }
}

GCHeap::GCHeap(size_t size_mb, NjsVM& vm)
    : vm(vm),
      heap_size(size_mb * 1024 * 1024),
      storage((byte *)malloc(size_mb * 1024 * 1024)),
      gc_thread(&GCHeap::minor_gc_task, this)
{
  newgen_start = storage;
  survivor1_start = newgen_start + size_t(newgen_size_ratio * heap_size);
  survivor2_start = survivor1_start + size_t(survivor_size_ratio * heap_size);
  oldgen_start = survivor2_start + size_t(survivor_size_ratio * heap_size);
  oldgen_end = oldgen_start + size_t(oldgen_size_ratio * heap_size);

  alloc_point = newgen_start;
  oldgen_alloc_point = oldgen_start;
  survivor_alloc_point = survivor1_start;
  survivor_from_start = survivor1_start;
  survivor_to_start = survivor2_start;

  dealloc_progress = survivor1_start;
  newgen_gc_threshold = newgen_start + size_t(newgen_gc_threshold_ratio * heap_size);

  record_set.reserve(1000);
}

GCHeap::~GCHeap() {
  gc_running.wait(true);
  {
    std::lock_guard<std::mutex> lock(cond_mutex);
    stop = true;
  }
  gc_cond_var.notify_one();
  gc_thread.join();

  newgen_dealloc_dead(newgen_start, alloc_point);
  newgen_dealloc_dead(survivor_from_start, survivor_alloc_point);
  oldgen_dealloc_dead(oldgen_start, oldgen_alloc_point);

  free(storage);
}

void GCHeap::gc_if_needed() {
  if (gc_pause_counter > 0) return;
  if (gc_requested) {
    gc_requested = false;
    gc();
  }
}

void GCHeap::gc() {
  gc_message("************ GC triggered ************");

  gc_running.wait(true);
  gc_running = true;
  {
    std::lock_guard<std::mutex> lock(cond_mutex);
    gc_start = true;
    copy_done = false;
  }
  gc_cond_var.notify_one();

  {
    std::unique_lock<std::mutex> lock(cond_mutex);
    gc_cond_var.wait(lock, [this] { return copy_done; });
  }

  gc_message("************  execution continue  ************");
}

void GCHeap::write_barrier(GCObject *obj, JSValue const& field) {
  if (not field.needs_gc()) return;
  write_barrier(obj, field.as_GCObject);
}

void GCHeap::write_barrier(GCObject *obj, GCObject *field) {
  field->ref_count_inc();

  if (obj >= reinterpret_cast<GCObject *>(oldgen_start)
      && field < reinterpret_cast<GCObject *>(oldgen_start)
      && not obj->gc_remembered) {
    record_set.push_back(obj);
    obj->gc_remembered = true;
  }
}

void GCHeap::minor_gc_task() {
  while (true) {
    std::unique_lock<std::mutex> lock(cond_mutex);
    gc_cond_var.wait(lock, [this] { return gc_start || stop; });
    if (stop) return;

    Timer timer("gc");
    gc_message("GC task start");

    stats.newgen_gc_count += 1;

    byte *prev_survivor_start = survivor_from_start;
    byte *prev_survivor_end = survivor_alloc_point;

    size_t prev_object_cnt = stats.newgen_object_cnt;
    size_t prev_usage = (alloc_point - newgen_start) + (survivor_alloc_point - survivor_from_start);
    stats.newgen_object_cnt = 0;
    // copy alive
    Timer timer_copy("copy alive");
    newgen_copy_alive();
    auto copy_time = timer_copy.end(Global::show_gc_statistics);
    stats.copy_time += copy_time;

    stats.newgen_last_gc_object_cnt = stats.newgen_object_cnt;
    stats.newgen_last_gc_usage = survivor_alloc_point - survivor_from_start;

    if (Global::show_gc_statistics) {
      std::cout << "GC copy done\n";
      std::cout << "newgen usage before GC: " << prev_usage / (1024 * 1024) << '\n';
      std::cout << "newgen usage after GC: " << stats.newgen_last_gc_usage / (1024 * 1024) << '\n';
      std::cout << "newgen object count before GC: " << prev_object_cnt << '\n';
      std::cout << "newgen object count after GC: " << stats.newgen_object_cnt << '\n';
      std::cout << "ratio: " << (double)stats.newgen_object_cnt / prev_object_cnt << '\n';
    }

    Timer timer_dealloc("dealloc dead");
    byte *prev_alloc_point = alloc_point;
    alloc_point = newgen_start;
    dealloc_progress = newgen_start;

    // let the mutator go
    gc_start = false;
    copy_done = true;
    lock.unlock();
    gc_cond_var.notify_one();
    // dealloc
    newgen_dealloc_dead_with_progress(newgen_start, prev_alloc_point);
    newgen_dealloc_dead(prev_survivor_start, prev_survivor_end);

    stats.dealloc_time += timer_dealloc.end(Global::show_gc_statistics);

    gc_message("GC dealloc done");
    roots.clear();
    gc_running = false;
    gc_running.notify_one();

    stats.total_time += timer.end(Global::show_gc_statistics);
  }

}

void GCHeap::major_gc() {
  mark_phase();
  // remove dead objects from the record set
  for (size_t i = 0; i < record_set.size(); ) {
    if (not record_set[i]->gc_visited) {
      record_set[i] = record_set.back();
      record_set.pop_back();
    } else {
      i += 1;
    }
  }
  sweep_phase();
}

void GCHeap::gather_roots() {
  if (const_roots.empty()) [[unlikely]] {
    const_roots.push_back(&vm.global_object.as_GCObject);
    const_roots.push_back(&vm.global_func.as_GCObject);

    const_roots.push_back(&vm.object_prototype.as_GCObject);
    const_roots.push_back(&vm.array_prototype.as_GCObject);
    const_roots.push_back(&vm.number_prototype.as_GCObject);
    const_roots.push_back(&vm.boolean_prototype.as_GCObject);
    const_roots.push_back(&vm.string_prototype.as_GCObject);
    const_roots.push_back(&vm.function_prototype.as_GCObject);
    const_roots.push_back(&vm.error_prototype.as_GCObject);
    const_roots.push_back(&vm.regexp_prototype.as_GCObject);
    const_roots.push_back(&vm.date_prototype.as_GCObject);
    const_roots.push_back(&vm.iterator_prototype.as_GCObject);

    for (auto& val : vm.native_error_protos) {
      const_roots.push_back(&val.as_GCObject);
    }

    for (auto& val : vm.string_const) {
      const_roots.push_back(&val.as_GCObject);
    }
  }

  // All values on the rt_stack are possible roots
  JSStackFrame *frame = vm.curr_frame;
  while (frame) {
    roots.push_back(&frame->function.as_GCObject);
    // do we need to check frame->alloc_cnt != 0 here?
    for (JSValue *val = frame->buffer; val <= *frame->sp_ref; val++) {
      if (val->needs_gc()) {
        roots.push_back(&val->as_GCObject);
      }
    }
    frame = frame->prev_frame;
  }

  for (auto& task : vm.micro_task_queue) {
    if (not task.use_native_func) {
      roots.push_back(&task.task_func.as_GCObject);
    }
    for (auto& val : task.args) {
      if (val.needs_gc()) {
        roots.push_back(&val.as_GCObject);
      }
    }
  }

  vm.runloop.gc_gather_roots(roots);
}

void GCHeap::newgen_copy_alive() {
  survivor_alloc_point = survivor_to_start;
  gather_roots();

#define COPY_TASK                                                               \
  if (*root < reinterpret_cast<GCObject *>(oldgen_start)) {                     \
    *root = copy_object(*root);                                                 \
  }

  for (GCObject **root : const_roots) {
    // only copy those in the new generation area
    COPY_TASK
  }
  for (GCObject **root : roots) {
    COPY_TASK
  }
  for (GCObject **root : vm.temp_roots) {
    COPY_TASK
  }

#undef COPY_TASK

  for (size_t i = 0; i < record_set.size(); ) {
    // update the fields of the objects in the record_set
    bool child_young = record_set[i]->gc_scan_children(*this);
    if (not child_young) {
      record_set[i]->gc_remembered = false;
      record_set[i] = record_set.back();
      record_set.pop_back();
    } else {
      i += 1;
    }
  }

  std::swap(survivor_from_start, survivor_to_start);
}

void GCHeap::newgen_dealloc_dead(byte *start, byte *end) {
  for (byte *ptr = start; ptr < end; ) {
    auto *obj = reinterpret_cast<GCObject *>(ptr);
    ptr += obj->size;
    if (obj->forward_ptr == nullptr) {
      obj->~GCObject();
    }
  }
}

void GCHeap::oldgen_dealloc_dead(byte *start, byte *end) {
  for (byte *ptr = start; ptr < end; ) {
    auto *obj = reinterpret_cast<GCObject *>(ptr);
    ptr += obj->size;
    if (not obj->gc_free) {
      obj->~GCObject();
    }
  }
}

void GCHeap::newgen_dealloc_dead_with_progress(byte *start, byte *end) {
  for (byte *ptr = start; ptr < end; ) {
    auto *obj = reinterpret_cast<GCObject *>(ptr);
    ptr += obj->size;
    if (obj->forward_ptr == nullptr) {
      obj->~GCObject();
    }
    dealloc_progress = ptr;
  }
  dealloc_progress = survivor1_start;
}

GCObject* GCHeap::copy_object(GCObject *obj) {
  GCObject *obj_new = obj->forward_ptr;
  if (obj_new == nullptr) {
    stats.newgen_object_cnt += 1;
    if (obj->gc_age < AGE_MAX) {
      obj_new = (GCObject *)survivor_alloc_point;
      survivor_alloc_point += obj->size;

      memcpy((void *)obj_new, (void *)obj, obj->size);
      obj->forward_ptr = obj_new;

      obj_new->gc_age += 1;
      obj_new->gc_scan_children(*this);
    }
    else {
      obj_new = promote(obj);
    }
  }
  assert(obj_new >= (GCObject *)survivor1_start);
  return obj_new;
}

GCObject* GCHeap::promote(GCObject *obj) {
  assert(obj->forward_ptr == nullptr);

  GCObject *obj_new = oldgen_alloc(obj->size);
  size_t actual_size = obj_new->size;

  stats.oldgen_object_cnt += 1;
  stats.oldgen_usage += actual_size;

  memcpy((void *)obj_new, (void *)obj, obj->size);
  obj_new->size = actual_size;
  obj_new->gc_free = false;
  obj_new->gc_visited = false;
  obj_new->gc_remembered = false;
  obj->forward_ptr = obj_new;

  if (obj_new->gc_has_young_child(reinterpret_cast<GCObject *>(oldgen_start))) {
    record_set.push_back(obj_new);
    obj_new->gc_remembered = true;
  }

  return obj_new;
}

GCObject* GCHeap::oldgen_alloc(size_t size) {
  GCObject* obj;
  if (oldgen_alloc_point + size <= oldgen_end) {
    obj = reinterpret_cast<GCObject *>(oldgen_alloc_point);
    oldgen_alloc_point += size;
    obj->size = size;
  }
  else {
    int free_list_index = size_to_index(size);
    int curr_index = free_list_index;
    bool did_gc = false;

    while (true) {
      auto& list = free_list[curr_index];
      for (GCObject *free_obj : list) {
        if (free_obj->size >= size) {
          obj = free_obj;
          goto slot_found;
        }
      }
      curr_index += 1;

      if (curr_index > 7) {
        if (not did_gc) {
          major_gc();
          // retry
          curr_index = free_list_index;
          did_gc = true;
        } else {
          fprintf(stderr, "memory allocation failed\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    slot_found:
    if (obj->size - size >= 40) [[unlikely]] {
      auto *divided = reinterpret_cast<GCObject *>(reinterpret_cast<byte *>(obj) + size);
      size_t divided_size = obj->size - size;
      divided->gc_free = true;
      divided->size = divided_size;
      free_list[size_to_index(divided_size)].push_back(divided);

      obj->size = size;
    }
  }

  return obj;
}

void GCHeap::mark_phase() {
#define MARK_TASK                                               \
  auto *gc_object = *root;                                      \
  if (not gc_object->gc_visited) {                              \
    gc_object->set_visited();                                   \
    gc_object->gc_mark_children();                              \
  }
  for (GCObject **root : roots) {
    MARK_TASK
  }
  for (GCObject **root : const_roots) {
    MARK_TASK
  }
  for (GCObject **root : vm.temp_roots) {
    MARK_TASK
  }
}

void GCHeap::sweep_phase() {
  byte *current = oldgen_start;

  while (current != oldgen_alloc_point) {
    auto *obj = reinterpret_cast<GCObject *>(current);
    if (not obj->gc_visited) {
      if (not obj->gc_free) {
        obj->~GCObject();
        obj->gc_free = true;
        int index = size_to_index(obj->size);
        free_list[index].push_back(obj);

        stats.oldgen_object_cnt -= 1;
        stats.oldgen_usage -= obj->size;
      }
    } else {
      obj->gc_visited = false;
    }

    current += obj->size;
  }
}

void GCHeap::check_fwd_pointer(byte *start, byte *end) {
  for (byte *ptr = start; ptr < end; ) {
    auto *obj = reinterpret_cast<GCObject *>(ptr);
    assert(obj->size % 8 == 0);
    assert(obj->forward_ptr == nullptr);
    ptr += obj->size;
  }
}

PrimitiveString* GCHeap::new_prim_string_ref(u16string_view str) {
  GCObject *ptr = newgen_alloc(sizeof(PrimitiveString));
  auto *prim_str = new (ptr) PrimitiveString(0);
  prim_str->init_with_ref(str.data(), str.size());
  ptr->size = sizeof(PrimitiveString);

  stats.newgen_object_cnt += 1;
  return prim_str;
}

PrimitiveString* GCHeap::new_prim_string(const char16_t *str, size_t length) {
  auto prim_str = new_prim_string_impl(length);
  prim_str->init(str, length);
  return prim_str;
}

PrimitiveString* GCHeap::new_prim_string(size_t length) {
  return new_prim_string_impl(length);
}

PrimitiveString* GCHeap::new_prim_string_impl(size_t length) {
  assert(length < UINT32_MAX);
  size_t capacity = 1.25 * (length + 1);
  size_t payload_size = capacity * CHAR_SIZE;
  size_t alloc_size = sizeof(PrimitiveString) + next_multiple_of_8(payload_size);

  GCObject *ptr = newgen_alloc(alloc_size);
  auto *prim_str = new (ptr) PrimitiveString(capacity);
  ptr->size = alloc_size;

  stats.newgen_object_cnt += 1;
  return prim_str;
}

HeapArray<JSValue>* GCHeap::new_array(u32 length) {
  size_t capacity = length;
  size_t payload_size = capacity * sizeof(JSValue);
  size_t alloc_size = sizeof(HeapArray<JSValue>) + next_multiple_of_8(payload_size);

  GCObject *ptr = newgen_alloc(alloc_size);
  auto *array = new (ptr) HeapArray<JSValue>(length, capacity);
  ptr->size = alloc_size;

  stats.newgen_object_cnt += 1;
  return array;
}

// Allocate memory for a new object.
GCObject* GCHeap::newgen_alloc(size_t size_byte) {
  byte *alloc_end = alloc_point + size_byte;
  // wait for the dealloc process to finish
  while (alloc_end > dealloc_progress) [[unlikely]] {}

  if (alloc_end > newgen_gc_threshold) [[unlikely]] {
    gc_requested = true;
    if (alloc_end > survivor1_start) [[unlikely]] {
      fprintf(stderr, "allocation failed\n");
      exit(1);
    }
  }

  auto *obj = reinterpret_cast<GCObject *>(alloc_point);
  alloc_point += size_byte;
  return obj;
}

void GCHeap::gc_message(string_view msg) {
  if (Global::show_gc_statistics) {
    std::cout << msg << '\n';
  }
}

}