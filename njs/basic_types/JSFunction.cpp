#include "JSFunction.h"

#include <string>
#include "njs/gc/GCHeap.h"

namespace njs {

JSFunction::JSFunction() : JSObject(ObjectClass::CLS_FUNCTION) {}

JSFunction::JSFunction(const u16string& name, u32 param_cnt, u32 local_var_cnt, u32 code_addr)
    : JSObject(ObjectClass::CLS_FUNCTION), name(name), param_count(param_cnt),
      local_var_count(local_var_cnt), code_address(code_addr) {}

JSFunction::~JSFunction() {
  JSObject::~JSObject();
  for (auto var : captured_var) {
    if (var.is_RCObject()) var.val.as_rc_object->release();
  }
}

void JSFunction::gc_scan_children(GCHeap& heap) {
  JSObject::gc_scan_children(heap);
  for (auto& var : captured_var) {
    assert(var.is_RCObject());
    if (var.is_RCObject() && var.deref().needs_gc()) {
      heap.gc_visit_object(var, var.as_GCObject());
    }
  }
}

std::string JSFunctionMeta::description() const {
  return "name_index: " + std::to_string(name_index) +
         ", code_address: " + std::to_string(code_address);
}

}