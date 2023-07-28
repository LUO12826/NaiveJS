#ifndef NJS_JSFUNCTION_H
#define NJS_JSFUNCTION_H

#include <string>
#include <span>
#include "JSValue.h"
#include "JSObject.h"
#include "njs/include/SmallVector.h"
#include "njs/common/ArrayRef.h"

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;
using std::u16string;
using llvm::SmallVector;

class GCHeap;
class NjsVM;
class JSFunction;
class JSFunctionMeta;

// Native function type. A Native function should act like a JavaScript function,
// accepting an array of arguments and returning a value.
using NativeFuncType = JSValue(*)(NjsVM&, JSFunction&, ArrayRef<JSValue>);

struct JSFunctionMeta {

  u32 name_index;
  bool is_anonymous {false};
  bool is_arrow_func {false};
  bool has_this_binding {false};
  bool is_native {false};

  u16 param_count;
  u16 local_var_count;
  u32 code_address;

  NativeFuncType native_func {nullptr};

  std::string description() const;
};

class JSFunction : public JSObject {
 public:
  JSFunction();
  JSFunction(u16string name, u32 param_cnt, u32 local_var_cnt, u32 code_addr);
  JSFunction(u16string name, u32 param_cnt);
  JSFunction(u16string name, const JSFunctionMeta& meta);
  explicit JSFunction(const JSFunctionMeta& meta);
  ~JSFunction() override;

  void gc_scan_children(GCHeap& heap) override;

  std::string description() override;

  std::u16string name;
  JSFunctionMeta meta;

  JSValue This {JSValue::undefined};
  std::vector<JSValue> captured_var;
  NativeFuncType native_func {nullptr};
};

} // namespace njs

#endif // NJS_JSFUNCTION_H