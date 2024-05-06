#include <cstring>
#include <iostream>

#include "GCHeap.h"
#include "njs/vm/NjsVM.h"
#include "njs/global_var.h"
#include "njs/utils/Timer.h"


namespace njs {

void GCHeap::gc() {
  if (Global::show_gc_statistics) {
    std::cout << "\033[33m";
    std::cout << "****************** gc starts ******************\n";
  }
  Timer timer("gc");

  Timer timer_copy("copy alive");
  byte *start = from_start;
  byte *end = alloc_point;
  copy_alive();
  timer_copy.end(Global::show_gc_statistics);

  Timer timer_dealloc("dealloc dead");
  dealloc_dead(start, end);
  timer_dealloc.end(Global::show_gc_statistics);

  if (Global::show_gc_statistics) {
    timer.end(true);
    std::cout << "******************  gc ends  ******************\n";
    std::cout << "\033[0m";
  }
  else {
    timer.end(false);
  }
}

void GCHeap::gc_visit_object(JSValue &handle, GCObject *obj) {
  assert(handle.needs_gc());
  GCObject *obj_new = copy_object(obj);
  handle.val.as_object = static_cast<JSObject*>(obj_new);
}

std::vector<JSValue *> GCHeap::gather_roots() {

  std::vector<JSValue *> roots;

  // All values on the rt_stack are possible roots
  for (JSValue *js_val = vm.rt_stack.data(); js_val < vm.sp; js_val++) {
    if (js_val->needs_gc()) roots.push_back(js_val);
    // STACK_FRAME_META1 contains the pointer to the function that owns the stack frame
    if (js_val->tag == JSValue::STACK_FRAME_META1) {
      roots.push_back(js_val);
    }
  }
  roots.push_back(&vm.global_object);

  roots.push_back(&vm.object_prototype);
  roots.push_back(&vm.array_prototype);
  roots.push_back(&vm.number_prototype);
  roots.push_back(&vm.boolean_prototype);
  roots.push_back(&vm.string_prototype);
  roots.push_back(&vm.function_prototype);

  auto task_roots = vm.runloop.gc_gather_roots();
  roots.insert(roots.end(), task_roots.begin(), task_roots.end());

  return roots;
}

void GCHeap::copy_alive() {
  alloc_point = to_start;
  std::vector<JSValue *> roots = gather_roots();

  if (Global::show_gc_statistics) {
    std::cout << "GC found roots:\n";
    if (roots.empty()) std::cout << "(empty)\n";
    for (JSValue *root : roots) {
      std::cout << root->as_GCObject()->description() << '\n';
    }
    std::cout << "---------------\n";
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
      if (Global::show_gc_statistics) {
        std::cout << "GC deallocate an object: "
                  << static_cast<JSObject *>(obj)->description() << '\n';
      }
      obj->~GCObject();
    }
  }
}

GCObject *GCHeap::copy_object(GCObject *obj) {
  GCObject *obj_new = obj->forward_ptr;
  if (obj_new == nullptr) {
    if (Global::show_gc_statistics) {
      std::cout << "Copy object: " << static_cast<JSObject *>(obj)->description() << '\n';
    }
    obj_new = (GCObject *)alloc_point;
    memcpy((void *)obj_new, (void *)obj, obj->size);
    obj->forward_ptr = obj_new;
    alloc_point += obj->size;

    obj_new->gc_scan_children(*this);
  }
  return obj_new;
}

// Allocate memory for a new object.
void *GCHeap::allocate(size_t size_byte) {
  if (lacking_free_memory(size_byte)) {
    gc();
    if (lacking_free_memory(size_byte)) {
      // allocation fail
    }
  }

  void *start_addr = alloc_point;
  alloc_point += size_byte;
  return start_addr;
}

}