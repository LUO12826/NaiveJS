#ifndef NJS_JS_BOUND_FUNCTION_H
#define NJS_JS_BOUND_FUNCTION_H

#include <vector>
#include "JSObject.h"

namespace njs {

using std::vector;

class GCHeap;
class NjsVM;

class JSBoundFunction : public JSObject {
 public:
  JSBoundFunction(NjsVM& vm, JSValue function);

  void set_this(NjsVM& vm, JSValue This);
  JSValue get_this();
  void set_args(NjsVM& vm, ArgRef argv);
  vector<JSValue>& get_args();

  Completion call(NjsVM& vm, JSValueRef This, JSValueRef new_target,
                  ArgRef argv, CallFlags flags);

  bool gc_scan_children(GCHeap& heap) override;
  void gc_mark_children() override;
  bool gc_has_young_child(GCObject *oldgen_start) override;

  u16string_view get_class_name() override {
    return u"BoundFunction";
  }

  std::string description() override;

 private:
  JSValue func;
  JSValue bound_this;
  vector<JSValue> args;
};

} // namespace njs


#endif // NJS_JS_BOUND_FUNCTION_H
