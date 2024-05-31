#ifndef NJS_JS_BOOLEAN_PROTOTYPE_H
#define NJS_JS_BOOLEAN_PROTOTYPE_H

#include "JSObject.h"
#include "JSBoolean.h"
#include "njs/vm/NjsVM.h"
#include "njs/vm/NativeFunction.h"
#include "njs/common/common_def.h"

namespace njs {

class JSBooleanPrototype : public JSObject {
 public:
  explicit JSBooleanPrototype(NjsVM &vm) : JSObject(ObjClass::CLS_NUMBER_PROTO) {
    add_method(vm, u"valueOf", JSBooleanPrototype::valueOf);
    add_method(vm, u"toString", JSBooleanPrototype::toString);
  }

  u16string_view get_class_name() override {
    return u"BooleanPrototype";
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is_object() && This.as_object()->get_class() == CLS_BOOLEAN) [[likely]] {
      assert(dynamic_cast<JSBoolean*>(This.as_object()));
      auto *bool_obj = static_cast<JSBoolean*>(This.as_object());
      return JSValue(bool_obj->value);
    }
    else if (This.is(JSValue::BOOLEAN)) {
      return This;
    }
    else {
      JSValue err = vm.build_error_internal(JS_TYPE_ERROR,
        u"Boolean.prototype.valueOf can only be called by boolean or boolean object.");
      return CompThrow(err);
    }
  }

  static Completion toString(vm_func_This_args_flags) {
    if (This.is_object() && This.as_object()->get_class() == CLS_BOOLEAN) [[likely]] {
      assert(dynamic_cast<JSBoolean*>(This.as_object()));
      auto *bool_obj = static_cast<JSBoolean*>(This.as_object());
      return vm.get_string_const(bool_obj->value ? AtomPool::k_true : AtomPool::k_false);
    }
    else if (This.is(JSValue::BOOLEAN)) {
      return vm.get_string_const(This.val.as_bool ? AtomPool::k_true : AtomPool::k_false);
    }
    else {
      return JSObjectPrototype::toString(JS_NATIVE_FUNC_ARGS);
    }
  }

};

} // namespace njs

#endif //NJS_JS_BOOLEAN_PROTOTYPE_H
