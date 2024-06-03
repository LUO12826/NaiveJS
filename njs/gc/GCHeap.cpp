#include <cstring>
#include <iostream>

#include "GCHeap.h"
#include "njs/vm/NjsVM.h"
#include "njs/global_var.h"
#include "njs/utils/Timer.h"


namespace njs {

void GCHeap::gc_if_needed() {
  if (object_cnt - last_gc_object_cnt > gc_threshold) {
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

void GCHeap::gc_task() {
  while (true) {
    std::unique_lock<std::mutex> lock(cond_mutex);
    gc_cond_var.wait(lock, [this] { return gc_start || stop; });
    if (stop) return;

    Timer timer("gc");
    gc_message("GC task start");

    stats.gc_count += 1;

    byte *start = from_start;
    byte *end = alloc_point;

    size_t old_object_cnt = object_cnt;
    object_cnt = 0;

    Timer timer_copy("copy alive");
    copy_alive();
    long long copy_time = timer_copy.end(Global::show_gc_statistics);
    stats.copy_time += copy_time;

    last_gc_object_cnt = object_cnt;
    if (object_cnt > 3 * old_object_cnt  / 4) {
      gc_threshold *= 2;
    } else {
      gc_threshold /= 2;
    }

    if (Global::show_gc_statistics) {
      std::cout << "GC copy done\n";
      std::cout << "object count before GC: " << old_object_cnt << '\n';
      std::cout << "object count after GC: " << object_cnt << '\n';
      std::cout << "ratio: " << (double)object_cnt / old_object_cnt << '\n';
      std::cout << "gc threshold: " << gc_threshold << '\n';
    }

    gc_start = false;
    copy_done = true;
    lock.unlock();
    gc_cond_var.notify_one();

    Timer timer_dealloc("dealloc dead");
    dealloc_dead(start, end);
    stats.dealloc_time += timer_dealloc.end(Global::show_gc_statistics);

    gc_message("GC dealloc done");

    gc_running = false;
    gc_running.notify_one();

    stats.total_time += timer.end(Global::show_gc_statistics);
  }

}

void GCHeap::gc_visit_object(JSValue& handle, GCObject *obj) {
  assert(handle.needs_gc());
  GCObject *obj_new = copy_object(obj);
  handle.u.as_object = static_cast<JSObject*>(obj_new);
}

vector<JSValue*> GCHeap::gather_roots() {
  vector<JSValue *> roots;

  // All values on the rt_stack are possible roots
  for (JSStackFrame *frame : vm.stack_frames) {
    roots.push_back(&frame->function);
    if (frame->alloc_cnt == 0) [[unlikely]] {
      continue;
    }
    for (JSValue *val = frame->buffer; val <= *frame->sp_ref; val++) {
      if (val->needs_gc()) {
        roots.push_back(val);
      }
    }
  }

  roots.push_back(&vm.global_object);
  roots.push_back(&vm.global_func);

  roots.push_back(&vm.object_prototype);
  roots.push_back(&vm.array_prototype);
  roots.push_back(&vm.number_prototype);
  roots.push_back(&vm.boolean_prototype);
  roots.push_back(&vm.string_prototype);
  roots.push_back(&vm.function_prototype);
  roots.push_back(&vm.error_prototype);
  roots.push_back(&vm.regexp_prototype);
  roots.push_back(&vm.date_prototype);
  roots.push_back(&vm.iterator_prototype);

  for (auto& val : vm.native_error_protos) {
    roots.push_back(&val);
  }

  for (auto& val : vm.string_const) {
    roots.push_back(&val);
  }

  for (auto& task : vm.micro_task_queue) {
    roots.push_back(&task.task_func);
    for (auto& val : task.args) {
      if (val.needs_gc()) {
        roots.push_back(&val);
      }
    }
  }

  auto task_roots = vm.runloop.gc_gather_roots();
  roots.insert(roots.end(), task_roots.begin(), task_roots.end());

  return roots;
}

void GCHeap::copy_alive() {
  alloc_point = to_start;
  vector<JSValue*> roots = gather_roots();

  if (false) {
    std::cout << "GC found roots:\n";
    if (roots.empty()) std::cout << "(empty)\n";
    for (JSValue *root : roots) {
      std::cout << root->as_GCObject()->description() << '\n';
    }
    std::cout << "---------------\n";
  }

  // for checking the index of this root in case the `forward_ptr` is not null
  int index = 0;
  for (JSValue *root : roots) {
    assert(root->as_GCObject()->forward_ptr == nullptr);
    index += 1;
  }
  for (JSValue *root : roots) {
    gc_visit_object(*root, root->as_GCObject());
  }

  std::swap(from_start, to_start);
}

void GCHeap::dealloc_dead(byte *start, byte *end) {
  for (byte *ptr = start; ptr < end; ) {
    GCObject *obj = reinterpret_cast<GCObject *>(ptr);
    ptr += obj->size;
    if (obj->forward_ptr == nullptr) {
//      if (Global::show_gc_statistics) [[unlikely]] {
//        std::cout << "GC deallocate an object: "
//                  << static_cast<JSObject *>(obj)->description() << '\n';
//      }
      obj->~GCObject();
    }
  }
}

GCObject* GCHeap::copy_object(GCObject *obj) {
  GCObject *obj_new = obj->forward_ptr;
  if (obj_new == nullptr) {
    object_cnt += 1;
//    if (Global::show_gc_statistics) [[unlikely]] {
//      std::cout << "Copy object: " << static_cast<JSObject *>(obj)->description() << '\n';
//    }
    obj_new = (GCObject *)alloc_point;
    memcpy((void *)obj_new, (void *)obj, obj->size);
    obj->forward_ptr = obj_new;
    alloc_point += obj->size;

    obj_new->gc_scan_children(*this);
  }
  return obj_new;
}

void GCHeap::check_fwd_pointer() {
  for (byte *ptr = from_start; ptr < alloc_point; ) {
    GCObject *obj = reinterpret_cast<GCObject *>(ptr);
    if (obj->size % 8 != 0) {
      assert(false);
    }
    if (obj->forward_ptr != nullptr) {
      assert(false);
    }
    ptr += obj->size;
  }
}
// Allocate memory for a new object.
void* GCHeap::allocate(size_t size_byte) {
  if (lacking_free_memory(size_byte)) [[unlikely]] {
    gc();
    if (lacking_free_memory(size_byte)) [[unlikely]]  {
      fprintf(stderr, "memory allocation failed\n");
      exit(EXIT_FAILURE);
    }
  }

  void *start_addr = alloc_point;
  alloc_point += size_byte;
  return start_addr;
}

void GCHeap::gc_message(string_view msg) {
  if (Global::show_gc_statistics) {
    std::cout << msg << '\n';
  }
}

}