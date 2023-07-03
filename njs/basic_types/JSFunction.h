#ifndef NJS_JSFUNCTION_H
#define NJS_JSFUNCTION_H

#include "JSObject.h"
#include <string>

namespace njs {

using u32 = uint32_t;

class GCHeap;

class JSFunction : public JSObject {
 public:
  JSFunction();

  JSFunction(const std::u16string& name, u32 code_addr);

  void gc_scan_children(GCHeap& heap) override;

  std::u16string name;
  bool is_anonymous {false};
  bool is_arrow_func {false};
  bool has_this_binding {false};
  u32 code_address;

};

struct JSFunctionMeta {

  u32 name_index;
  bool is_anonymous {false};
  bool is_arrow_func {false};
  bool has_this_binding {false};
  u32 code_address;

  std::string description() const;
};

} // namespace njs

#endif // NJS_JSFUNCTION_H