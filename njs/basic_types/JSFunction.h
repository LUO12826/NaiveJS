#ifndef NJS_JSFUNCTION_H
#define NJS_JSFUNCTION_H

#include <string>
#include "JSValue.h"
#include "JSObject.h"
#include "JSFunctionMeta.h"

namespace njs {

using std::u16string;

class GCHeap;
class NjsVM;

class JSFunction : public JSObject {
 public:
  JSFunction(NjsVM& vm, u16string_view name, JSFunctionMeta *meta);
  JSFunction(NjsVM& vm, JSFunctionMeta *meta);

  bool gc_scan_children(GCHeap& heap) override;
  void gc_mark_children() override;
  bool gc_has_young_child(GCObject *oldgen_start) override;

  bool is_native() { return native_func != nullptr; }

  u16string_view get_class_name() override {
    return u"Function";
  }

  std::string description() override;

  u16string_view name;
  JSFunctionMeta *meta {nullptr};
  // copy from meta
  bool prepare_arguments_array {false};
  u16 param_count;
  u16 local_var_count;
  u16 stack_size;
  u32 bytecode_start;

  bool is_arrow_func {false};
  bool has_auxiliary_data {false};
  JSValue this_or_auxiliary_data;
  std::vector<JSValue> captured_var;
  NativeFuncType native_func {nullptr};
};

} // namespace njs

#endif // NJS_JSFUNCTION_H