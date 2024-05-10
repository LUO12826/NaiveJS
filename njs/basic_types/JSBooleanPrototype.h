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
  explicit JSBooleanPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_NUMBER_PROTO) {
    add_method(vm, u"valueOf", JSBooleanPrototype::valueOf);
  }

  u16string_view get_class_name() override {
    return u"BooleanPrototype";
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is(JSValue::BOOLEAN)) {
      return This;
    }
    else if (This.is_object() && This.as_object()->obj_class == ObjectClass::CLS_BOOLEAN) {
      assert(dynamic_cast<JSBoolean*>(This.as_object()) != nullptr);
      auto *bool_obj = static_cast<JSBoolean*>(This.as_object());
      return JSValue(bool_obj->value);
    }
    else {
      JSValue err = NativeFunctions::build_error_internal(vm, u"Boolean.prototype.valueOf can only accept argument "
                                                               "of type boolean or boolean object.");
      return Completion::with_throw(err);
    }
  }

};

} // namespace njs

#endif //NJS_JS_BOOLEAN_PROTOTYPE_H
