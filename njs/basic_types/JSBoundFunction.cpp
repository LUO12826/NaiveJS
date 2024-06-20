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
                                 Span<JSValue> argv, CallFlags flags) {
  NOGC;
  HANDLE_COLLECTOR;
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
  gc_handle_add(actual_argv);
  nogc.resume_gc();

  JSValueRef actual_func = bound_chain.back()->func;

  if (flags.constructor) {
    JSValue proto = TRYCC(actual_func.as_object->get_prop(vm, AtomPool::k_prototype));
    proto = proto.is_object() ? proto : vm.object_prototype;
    const_cast<JSValue&>(This).set_val(vm.heap.new_object<JSObject>(vm, CLS_OBJECT, proto));
  }

  return vm.call_internal(
      actual_func,
      unlikely(flags.constructor) ? This : bound_this,
      new_target,
      actual_argv,
      CallFlags()
  );
}

void JSBoundFunction::set_args(NjsVM& vm, Span<JSValue> argv) {
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
  gc_check_and_visit_object(child_young, bound_this);
  for (auto& val : args) {
    gc_check_and_visit_object(child_young, val);
  }
  return child_young;
}

void JSBoundFunction::gc_mark_children() {
  JSObject::gc_mark_children();
  gc_mark_object(func.as_GCObject);
  gc_check_and_mark_object(bound_this);

  for (auto& val : args) {
    gc_check_and_mark_object(val);
  }
}

bool JSBoundFunction::gc_has_young_child(njs::GCObject *oldgen_start) {
  if (JSObject::gc_has_young_child(oldgen_start)) return true;
  if (func.as_GCObject < oldgen_start) return true;
  gc_check_object_young(bound_this);
  for (auto& val : args) {
    gc_check_object_young(val);
  }
  return false;
}

std::string JSBoundFunction::description() {
  return "(bound)" + func.as_func->description();
}

}
