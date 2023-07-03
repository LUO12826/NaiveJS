#include "JSFunction.h"

#include <string>

namespace njs {

JSFunction::JSFunction() : JSObject(ObjectClass::CLS_FUNCTION) {}

JSFunction::JSFunction(const std::u16string& name, u32 code_addr)
    : JSObject(ObjectClass::CLS_FUNCTION), name(name), code_address(code_addr) {}

void JSFunction::gc_scan_children(GCHeap& heap) {
  JSObject::gc_scan_children(heap);
}

std::string JSFunctionMeta::description() const {
  return "name_index: " + std::to_string(name_index) +
         ", code_address: " + std::to_string(code_address);
}

}