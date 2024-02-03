#ifndef NJS_JS_BOOLEAN_PROTOTYPE_H
#define NJS_JS_BOOLEAN_PROTOTYPE_H

#include "JSObject.h"
#include "JSBoolean.h"
#include "njs/vm/NjsVM.h"
#include "njs/vm/NativeFunction.h"

namespace njs {

class JSBooleanPrototype : public JSObject {
 public:
  explicit JSBooleanPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_NUMBER_PROTO) {
    add_method(vm, u"valueOf", JSBooleanPrototype::valueOf);
  }

  u16string_view get_class_name() override {
    return u"BooleanPrototype";
  }

  static Completion valueOf(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    if (func.This.is(JSValue::BOOLEAN)) {
      return func.This;
    }
    else if (func.This.is_object() && func.This.as_object()->obj_class == ObjectClass::CLS_BOOLEAN) {
      assert(dynamic_cast<JSBoolean*>(func.This.as_object()) != nullptr);
      auto *bool_obj = static_cast<JSBoolean*>(func.This.as_object());
      return JSValue(bool_obj->value);
    }
    else {
      JSValue err = InternalFunctions::build_error_internal(vm,u"Boolean.prototype.valueOf can only accept argument "
                                                               "of type boolean or boolean object.");
      return Completion::with_throw(err);
    }
  }

};

} // namespace njs

#endif //NJS_JS_BOOLEAN_PROTOTYPE_H
