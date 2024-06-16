#include "JSBoundFunction.h"

#include <ranges>
#include "njs/vm/NjsVM.h"
#include "njs/include/SmallVector.h"

namespace njs {

using llvm::SmallVector;

JSBoundFunction::JSBoundFunction(NjsVM& vm, JSValue function) : JSObject(CLS_BOUND_FUNCTION) {
  WRITE_BARRIER(function);
  func = function;
}

void JSBoundFunction::set_this(NjsVM& vm, JSValue This) {
  WRITE_BARRIER(This);
  bound_this = This;
}

JSValue JSBoundFunction::get_this() {
  return bound_this;
}

vector<JSValue>& JSBoundFunction::get_args() {
  return args;
}

Completion JSBoundFunction::call(NjsVM& vm, JSValueRef This, JSValueRef new_target,
                                 ArrayRef<JSValue> argv, CallFlags flags) {
  SmallVector<JSBoundFunction *, 2> bound_chain;
  bound_chain.push_back(this);
  JSValue iter = func;
  while (iter.as_object->get_class() == CLS_BOUND_FUNCTION) {
    auto *bound_func = iter.as_Object<JSBoundFunction>();
    bound_chain.push_back(bound_func);
    iter = bound_func->func;
  }

  vector<JSValue> actual_argv;
  for (auto bf : std::ranges::reverse_view(bound_chain)) {
    actual_argv.insert(actual_argv.end(), bf->args.begin(), bf->args.end());
  }
  actual_argv.insert(actual_argv.end(), argv.data(), argv.data() + argv.size());

  return vm.call_internal(
      bound_chain.back()->func,
      unlikely(flags.constructor) ? This : bound_this,
      new_target,
      actual_argv,
      flags
  );
}

void JSBoundFunction::set_args(NjsVM& vm, ArrayRef<JSValue> argv) {
  this->args.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); i++) {
    this->args.push_back(argv[i]);
    WRITE_BARRIER(argv[i]);
  }
}

bool JSBoundFunction::gc_scan_children(njs::GCHeap& heap) {
  bool child_young = false;
  child_young |= JSObject::gc_scan_children(heap);
  child_young |= heap.gc_visit_object(func);
  if (bound_this.needs_gc()) {
    child_young |= heap.gc_visit_object(bound_this);
  }
  for (auto& val : args) {
    if (val.needs_gc()) {
      child_young |= heap.gc_visit_object(val);
    }
  }
  return child_young;
}

void JSBoundFunction::gc_mark_children() {
  JSObject::gc_mark_children();
  gc_mark_object(func.as_GCObject);
  if (bound_this.needs_gc()) {
    gc_mark_object(bound_this.as_GCObject);
  }
  for (auto& val : args) {
    if (val.needs_gc()) {
      gc_mark_object(val.as_GCObject);
    }
  }
}

bool JSBoundFunction::gc_has_young_child(njs::GCObject *oldgen_start) {
  if (JSObject::gc_has_young_child(oldgen_start)) return true;
  if (func.as_GCObject < oldgen_start) return true;
  if (bound_this.needs_gc()) {
    if (bound_this.as_GCObject < oldgen_start) return true;
  }
  for (auto& val : args) {
    if (val.needs_gc()) {
      if (val.as_GCObject < oldgen_start) return true;
    }
  }
  return false;
}

std::string JSBoundFunction::description() {
  return "(bound)" + func.as_func->description();
}

}
