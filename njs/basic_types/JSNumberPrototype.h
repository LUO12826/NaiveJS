#ifndef NJS_JS_NUMBER_PROTOTYPE_H
#define NJS_JS_NUMBER_PROTOTYPE_H

#include "JSObject.h"
#include "JSObjectPrototype.h"
#include "JSNumber.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/common_def.h"
#include "njs/common/conversion_helper.h"

namespace njs {

class JSNumberPrototype : public JSObject {
 public:
  explicit JSNumberPrototype(NjsVM &vm) : JSObject(CLS_NUMBER_PROTO) {
    add_method(vm, u"valueOf", JSNumberPrototype::valueOf);
    add_method(vm, u"toString", JSNumberPrototype::toString);
  }

  u16string_view get_class_name() override {
    return u"NumberPrototype";
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_NUMBER) [[likely]] {
      assert(dynamic_cast<JSNumber*>(This.as_object));
      auto *num_obj = static_cast<JSNumber*>(This.as_object);
      return JSValue(num_obj->value);
    }
    else if (This.is(JSValue::NUM_FLOAT)) {
      return This;
    }
    else {
      JSValue err = vm.build_error_internal(JS_TYPE_ERROR,
        u"Number.prototype.valueOf can only be called by number or number object.");
      return CompThrow(err);
    }
  }

  static Completion toString(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_NUMBER) [[likely]] {
      assert(dynamic_cast<JSNumber*>(This.as_object));
      auto *num_obj = static_cast<JSNumber*>(This.as_object);
      return vm.new_primitive_string(New::double_to_u16string(num_obj->value));
    }
    else if (This.is(JSValue::NUM_FLOAT)) {
      return vm.new_primitive_string(New::double_to_u16string(This.as_f64));
    }
    else {
      return JSObjectPrototype::toString(JS_NATIVE_FUNC_ARGS);
    }
  }
};

} // namespace njs

#endif //NJS_JS_NUMBER_PROTOTYPE_H
