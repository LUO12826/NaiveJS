#ifndef NJS_JSFUNCTION_H
#define NJS_JSFUNCTION_H

#include "JSObject.h"
#include <string>

namespace njs {

using u32 = uint32_t;

class GCHeap;

class JSFunction : public JSObject {
 public:
  JSFunction() : JSObject(ObjectClass::CLS_FUNCTION) {}

  void gc_scan_children(GCHeap& heap) override;

  std::u16string name;
  bool is_anonymous;
  bool is_arrow_func;
  bool has_this_binding;
  u32 code_address;

};

} // namespace njs

#endif // NJS_JSFUNCTION_H