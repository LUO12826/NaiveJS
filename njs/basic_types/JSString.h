#ifndef NJS_JSSTRING_H
#define NJS_JSSTRING_H

#include "JSObject.h"
#include "PrimitiveString.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSString : public JSObject {
 public:
  JSString(NjsVM& vm, const PrimitiveString& str) :
      JSObject(CLS_STRING, vm.string_prototype),
      value(str.str) {}

  u16string_view get_class_name() override {
    return u"String";
  }

  Completion get_property_impl(NjsVM &vm, JSValue key) override {
    JSValue k = TRY_COMP(js_to_property_key(vm, key));
    if (k.is_atom() && k.u.as_atom == AtomPool::k_length) {
      return JSFloat(value.length());
    } else {
      return get_prop(vm, k);
    }
  }

  ErrorOr<bool> set_property_impl(NjsVM &vm, JSValue key, JSValue val) override {
    JSValue k = TRY_ERR(js_to_property_key(vm, key));
    if (k.is_atom() && k.u.as_atom == AtomPool::k_length) {
      // do nothing. TODO: check this
      return true;
    } else {
      return set_prop(vm, k, val);
    }
  }

//  std::string description() override;
//  std::string to_string(NjsVM& vm) override;
//  void to_json(u16string& output, NjsVM& vm) const override;

  PrimitiveString value;
};

} // namespace njs

#endif //NJS_JSSTRING_H
