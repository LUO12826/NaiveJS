#ifndef NJS_JSFUNCTION_H
#define NJS_JSFUNCTION_H

#include <string>
#include <span>
#include "JSValue.h"
#include "JSObject.h"
#include "njs/common/ArrayRef.h"
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

  u16string_view get_class_name() override {
    return u"Function";
  }

  std::string description() override;

  u16string_view name;
  JSFunctionMeta *meta {nullptr};

  bool has_this_binding {false};
  JSValue this_binding;
  std::vector<JSValue> captured_var;
  NativeFuncType native_func {nullptr};
};

} // namespace njs

#endif // NJS_JSFUNCTION_H