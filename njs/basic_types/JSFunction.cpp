#include "JSFunction.h"

#include <string>
#include <utility>
#include "njs/gc/GCHeap.h"
#include "njs/vm/NjsVM.h"

namespace njs {
JSFunction::JSFunction(NjsVM& vm, u16string_view name, JSFunctionMeta *meta)
    : JSObject(vm, CLS_FUNCTION, vm.function_prototype)
    , name(name)
    , meta(meta)
    , native_func(meta->native_func) {}

JSFunction::JSFunction(NjsVM& vm, JSFunctionMeta *meta)
    : JSFunction(vm, u"", meta) {}

bool JSFunction::gc_scan_children(GCHeap& heap) {
  bool child_young = false;
  child_young |= JSObject::gc_scan_children(heap);
  for (auto& var : captured_var) {
    assert(var.is(JSValue::HEAP_VAL));
    child_young |= heap.gc_visit_object2(var, var.as_GCObject);
  }
  if (this_binding.needs_gc()) {
    child_young |= heap.gc_visit_object2(this_binding, this_binding.as_GCObject);
  }
  return child_young;
}

void JSFunction::gc_mark_children() {
  JSObject::gc_mark_children();
  for (auto& var : captured_var) {
    assert(var.is(JSValue::HEAP_VAL));
    gc_mark_object(var.as_GCObject);
  }
  if (this_binding.needs_gc()) {
    gc_mark_object(this_binding.as_GCObject);
  }
}

bool JSFunction::gc_has_young_child(GCObject *oldgen_start) {
  if (JSObject::gc_has_young_child(oldgen_start)) return true;
  for (auto& var : captured_var) {
    assert(var.is(JSValue::HEAP_VAL));
    if (var.as_GCObject < oldgen_start) return true;
  }
  if (this_binding.needs_gc()) {
    if (this_binding.as_GCObject < oldgen_start) return true;
  }
  return false;
}

std::string JSFunction::description() {
  std::string desc;
  desc += "JSFunction ";
  if (meta->is_anonymous) desc += "(anonymous)";
  else {
    desc += "named: ";
    desc += to_u8string(name);
  }

  desc += ", is native: ";
  desc += to_u8string(meta->is_native);
  desc += "  Props: ";
  desc += JSObject::description();

  return desc;
}

std::string JSFunctionMeta::description() const {
  std::string desc;
  if (is_anonymous) desc += "(anonymous)";
  else {
    desc += "name_index: " + std::to_string(name_index);
  }
  desc += ", bytecode_start: " + std::to_string(bytecode_start);

  return desc;
}

}