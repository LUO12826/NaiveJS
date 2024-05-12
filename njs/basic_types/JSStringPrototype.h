#ifndef NJS_JS_STRING_PROTOTYPE_H
#define NJS_JS_STRING_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/ArrayRef.h"
#include "JSFunction.h"
#include "JSString.h"
#include "njs/vm/Completion.h"

namespace njs {

class JSStringPrototype : public JSObject {
 public:
  explicit JSStringPrototype(NjsVM &vm) : JSObject(ObjClass::CLS_STRING_PROTO) {
    add_method(vm, u"charAt", JSStringPrototype::char_at);
    add_method(vm, u"valueOf", JSStringPrototype::valueOf);
  }

  u16string_view get_class_name() override {
    return u"StringPrototype";
  }

  static Completion char_at(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    assert(This.is(JSValue::STRING) || This.is(JSValue::STRING_OBJ));

    u16string& str = This.is(JSValue::STRING) ? This.val.as_prim_string->str
                                              : This.val.as_string->value.str;

    double index = args[0].val.as_f64;
    if (index < 0 || index > str.size()) {
      return vm.new_primitive_string(u"");
    }
    return vm.new_primitive_string(u16string{str[(size_t)index]});
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is(JSValue::STRING)) {
      return This;
    }
    else if (This.is_object() && This.as_object()->get_class() == ObjClass::CLS_STRING) {
      assert(dynamic_cast<JSString*>(This.as_object()) != nullptr);
      auto *str_obj = static_cast<JSString*>(This.as_object());
      return vm.new_primitive_string(str_obj->value.str);
    }
    else {
      JSValue err = vm.build_error_internal(u"String.prototype.valueOf can only accept argument "
                                             "of type string or string object.");
      return Completion::with_throw(err);
    }
  }

};

} // namespace njs

#endif //NJS_JS_STRING_PROTOTYPE_H
