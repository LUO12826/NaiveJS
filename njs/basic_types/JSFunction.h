#ifndef NJS_JSFUNCTION_H
#define NJS_JSFUNCTION_H

#include <string>
#include "JSValue.h"
#include "JSObject.h"
#include "njs/include/SmallVector.h"

namespace njs {

using u32 = uint32_t;
using std::u16string;
using llvm::SmallVector;

class GCHeap;
class JSFunctionMeta;

class JSFunction : public JSObject {
 public:
  JSFunction();
  JSFunction(const u16string& name, u32 param_cnt, u32 local_var_cnt, u32 code_addr);
  ~JSFunction();

  void gc_scan_children(GCHeap& heap) override;

  std::u16string name;
  bool is_anonymous {false};
  bool is_arrow_func {false};
  bool has_this_binding {false};
  u32 param_count;
  u32 local_var_count;
  u32 code_address;
  SmallVector<JSValue, 3> captured_var;
};

struct JSFunctionMeta {

  u32 name_index;
  bool is_anonymous {false};
  bool is_arrow_func {false};
  bool has_this_binding {false};
  u32 param_count;
  u32 local_var_count;
  u32 code_address;

  std::string description() const;
};

} // namespace njs

#endif // NJS_JSFUNCTION_H