#ifndef NJS_JSFUNCTION_H
#define NJS_JSFUNCTION_H

#include <string>
#include "JSValue.h"
#include "JSObject.h"
#include "JSFunctionMeta.h"
#include "HeapArray.h"
#include "njs/common/common_def.h"

namespace njs {

using std::u16string;

class GCHeap;
class NjsVM;

class JSFunction : public JSObject {
 public:

  static inline JSFunctionMeta *async_fulfill_callback_meta;
  static inline JSFunctionMeta *async_reject_callback_meta;

  static void add_internal_function_meta(NjsVM& vm);
  static array<JSValue, 2> build_async_then_callback(NjsVM& vm, JSValueRef promise);
  static Completion async_then_callback(vm_func_This_args_flags);
  ResumableFuncState* build_exec_state(NjsVM& vm, JSValueRef This, ArgRef argv);

  JSFunction(NjsVM& vm, u16string_view name, JSFunctionMeta *meta);
  JSFunction(NjsVM& vm, JSFunctionMeta *meta);

  bool gc_scan_children(GCHeap& heap) override;
  void gc_mark_children() override;
  bool gc_has_young_child(GCObject *oldgen_start) override;

  bool is_native() { return native_func != nullptr; }
  bool is_async() {
    return get_class() == CLS_ASYNC_FUNC | get_class() == CLS_ASYNC_GENERATOR_FUNC;
  }
  bool is_generator() {
    return get_class() == CLS_GENERATOR_FUNC | get_class() == CLS_ASYNC_GENERATOR_FUNC;
  }

  HeapArray<JSValue>& get_captured_var() {
    return *captured_var.as_heap_array;
  }

  u16string_view get_class_name() override {
    return u"Function";
  }

  std::string description() override;

  u16string_view name;
  JSFunctionMeta *meta {nullptr};
  // copy from meta
  bool need_arguments_array {false};
  u16 param_count;
  u16 local_var_count;
  u16 stack_size;
  u32 bytecode_start;

  bool is_constructor {true};
  bool is_arrow_func {false};
  bool has_auxiliary_data {false};
  JSValue this_or_auxiliary_data;
  // handle for a heap array
  JSValue captured_var;
  NativeFuncType native_func {nullptr};
};

} // namespace njs

#endif // NJS_JSFUNCTION_H