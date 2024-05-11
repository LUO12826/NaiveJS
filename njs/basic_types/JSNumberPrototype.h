#ifndef NJS_JS_NUMBER_PROTOTYPE_H
#define NJS_JS_NUMBER_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "JSNumber.h"
#include "njs/common/common_def.h"

namespace njs {

class JSNumberPrototype : public JSObject {
 public:
  explicit JSNumberPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_NUMBER_PROTO) {
    add_method(vm, u"valueOf", JSNumberPrototype::valueOf);
  }

  u16string_view get_class_name() override {
    return u"NumberPrototype";
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is(JSValue::NUM_FLOAT)) {
      return This;
    }
    else if (This.is_object() && This.as_object()->obj_class == ObjectClass::CLS_NUMBER) {
      assert(dynamic_cast<JSNumber*>(This.as_object()) != nullptr);
      auto *num_obj = static_cast<JSNumber*>(This.as_object());
      return JSValue(num_obj->value);
    }
    else {
      JSValue err = NativeFunctions::build_error_internal(vm, u"Number.prototype.valueOf can only accept argument "
                                                               "of type number or number object.");
      return Completion::with_throw(err);
    }
  }
};

} // namespace njs

#endif //NJS_JS_NUMBER_PROTOTYPE_H
