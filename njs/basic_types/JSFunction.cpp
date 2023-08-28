#include "JSFunction.h"

#include <string>
#include <utility>
#include "njs/gc/GCHeap.h"

namespace njs {

JSFunction::JSFunction() : JSObject(ObjectClass::CLS_FUNCTION) {}

JSFunction::JSFunction(u16string name, const JSFunctionMeta& meta) : JSFunction(meta) {
  this->name = std::move(name);
}

JSFunction::JSFunction(const JSFunctionMeta& meta): JSObject(ObjectClass::CLS_FUNCTION) {
  this->meta = meta;
  native_func = meta.native_func;
}

JSFunction::~JSFunction() {
  JSObject::~JSObject();
  for (auto var : captured_var) {
    if (var.is_RCObject()) var.val.as_RCObject->release();
  }
}

void JSFunction::gc_scan_children(GCHeap& heap) {
  JSObject::gc_scan_children(heap);
  for (auto& var : captured_var) {
    assert(var.tag_is(JSValue::HEAP_VAL));
    JSValue& the_value = var.deref();

    if (the_value.needs_gc()) {
      heap.gc_visit_object(the_value, the_value.as_GCObject());
    }
  }
  if (This.needs_gc()) heap.gc_visit_object(This, This.as_GCObject());
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
  desc += ", code_address: " + std::to_string(code_address);

  return desc;
}

}