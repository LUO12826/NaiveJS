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

class JSFunction : public JSObject {
 public:
  JSFunction();
  JSFunction(u16string name, const JSFunctionMeta& meta);
  explicit JSFunction(const JSFunctionMeta& meta);
  ~JSFunction() override;

  void gc_scan_children(GCHeap& heap) override;

  u16string_view get_class_name() override {
    return u"Function";
  }

  std::string description() override;

  std::u16string name;
  JSFunctionMeta meta;

  bool has_this_binding {false};
  JSValue This {JSValue::undefined};
  std::vector<JSValue> captured_var;
  NativeFuncType native_func {nullptr};
};

} // namespace njs

#endif // NJS_JSFUNCTION_H