#include "JSFunction.h"

#include <string>
#include <utility>
#include "njs/gc/GCHeap.h"
#include "njs/vm/NjsVM.h"

namespace njs {
JSFunction::JSFunction(NjsVM& vm, u16string name, const JSFunctionMeta& meta)
    : JSObject(CLS_FUNCTION, vm.function_prototype)
    , name(std::move(name))
    , meta(meta)
    , native_func(meta.native_func) {}

JSFunction::JSFunction(NjsVM& vm, const JSFunctionMeta& meta)
    : JSFunction(vm, u"", meta) {}

void JSFunction::gc_scan_children(GCHeap& heap) {
  JSObject::gc_scan_children(heap);
  for (auto& var : captured_var) {
    assert(var.is(JSValue::HEAP_VAL));
    heap.gc_visit_object(var, var.as_GCObject());
  }
  if (this_binding.needs_gc()) {
    heap.gc_visit_object(this_binding, this_binding.as_GCObject());
  }
}

std::string JSFunction::description() {
  std::string desc;
  desc += "JSFunction ";
  if (meta.is_anonymous) desc += "(anonymous)";
  else {
    desc += "named: ";
    desc += to_u8string(name);
  }

  desc += ", is native: ";
  desc += to_u8string(meta.is_native);
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