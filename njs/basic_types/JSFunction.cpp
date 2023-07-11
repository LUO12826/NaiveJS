#include "JSFunction.h"

#include <string>
#include <utility>
#include "njs/gc/GCHeap.h"

namespace njs {

JSFunction::JSFunction() : JSObject(ObjectClass::CLS_FUNCTION) {}

JSFunction::JSFunction(u16string name, u32 param_cnt, u32 local_var_cnt, u32 code_addr)
    : JSObject(ObjectClass::CLS_FUNCTION), name(std::move(name)), param_count(param_cnt),
      local_var_count(local_var_cnt), code_address(code_addr) {}

JSFunction::JSFunction(u16string name, u32 param_cnt)
    : JSObject(ObjectClass::CLS_FUNCTION), name(std::move(name)), param_count(param_cnt) {}

JSFunction::JSFunction(u16string name, const JSFunctionMeta& meta)
    : JSObject(ObjectClass::CLS_FUNCTION), name(std::move(name)) {
  param_count = meta.param_count;
  local_var_count = meta.local_var_count;
  code_address = meta.code_address;

  is_anonymous = meta.is_anonymous;
  is_arrow_func = meta.is_arrow_func;
  has_this_binding = meta.has_this_binding;
  is_native = meta.is_native;

  native_func = meta.native_func;
}

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