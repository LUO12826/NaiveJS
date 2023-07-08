#include "GCHeap.h"
#include "njs/vm/NjsVM.h"
#include "njs/global_var.h"
#include "njs/utils/Timer.h"

#include <iostream>

namespace njs {

void GCHeap::gc() {
  Timer timer("gc");
  if (Global::show_gc_statistics) {
    std::cout << "****************** gc starts ******************" << std::endl;
  }
  copy_alive();

  if (Global::show_gc_statistics) {
    std::cout << "******************  gc ends  ******************" << std::endl;
    timer.end(true);
  }
  else {
    timer.end(false);
  }
}

void GCHeap::gc_visit_object(JSValue &handle, GCObject *obj) {
  GCObject *obj_new = copy_object(obj);
  handle.val.as_object = static_cast<JSObject*>(obj_new);
}

std::vector<JSValue *> GCHeap::gather_roots() {

  std::vector<JSValue *> roots;

  for (u32 i = 0; i < vm.sp; i++) {
    JSValue& js_val = vm.rt_stack[i];

    if (js_val.tag > JSValue::NEED_GC_BEGIN && js_val.tag < JSValue::NEED_GC_END) {
      roots.push_back(&js_val);
    }
    if (js_val.tag == JSValue::STACK_FRAME_META1) {
      roots.push_back(&js_val);
    }
  }

  return roots;
}

void GCHeap::copy_alive() {
  alloc_point = to_start;
  std::vector<JSValue *> roots = gather_roots();

  if (Global::show_gc_statistics) {
    std::cout << "GC found roots:" << std::endl;
    for (JSValue *root : roots) {
      std::cout << root->as_GCObject()->description() << std::endl;
    }
  }

  for (JSValue *root : roots) {
    gc_visit_object(*root, root->as_GCObject());
  }

  std::swap(from_start, to_start);
}

GCObject *GCHeap::copy_object(GCObject *obj) {
  if (obj->forward_ptr == nullptr) {
    std::cout << "copy an object" << std::endl;
    memcpy(alloc_point, (void *)obj, obj->size);
    obj->forward_ptr = (GCObject *)alloc_point;
    alloc_point += obj->size;

    obj->gc_scan_children(*this);
  }
  return obj->forward_ptr;
}

// Allocate memory for a new object.
void *GCHeap::allocate(size_t size_byte) {
  if (lacking_free_memory(size_byte)) {
    // This call starts GC.
    copy_alive();
    if (lacking_free_memory(size_byte)) {
      // allocation fail
    }
  }

  void *start_addr = alloc_point;
  alloc_point += size_byte;
  return start_addr;
}

}