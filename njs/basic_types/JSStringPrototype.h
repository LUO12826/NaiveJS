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
  explicit JSStringPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_STRING_PROTO) {
    add_method(vm, u"charAt", JSStringPrototype::char_at);
    add_method(vm, u"valueOf", JSStringPrototype::valueOf);
  }

  u16string_view get_class_name() override {
    return u"StringPrototype";
  }

  static Completion char_at(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    assert(func.This.is(JSValue::STRING) || func.This.is(JSValue::STRING_OBJ));

    u16string& str = func.This.is(JSValue::STRING) ? func.This.val.as_primitive_string->str
                                                   : func.This.val.as_string->value.str;

    double index = args[0].val.as_f64;
    if (index < 0 || index > str.size()) {
      return JSValue(vm.heap.new_object<PrimitiveString>(u""));
    }
    auto *prim_str = vm.heap.new_object<PrimitiveString>(u16string{str[(size_t)index]});
    return JSValue(prim_str);
  }

  static Completion valueOf(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    if (func.This.is(JSValue::STRING)) {
      return func.This;
    }
    else if (func.This.is_object() && func.This.as_object()->obj_class == ObjectClass::CLS_STRING) {
      assert(dynamic_cast<JSString*>(func.This.as_object()) != nullptr);
      auto *str_obj = static_cast<JSString*>(func.This.as_object());
      auto prim_string = vm.heap.new_object<PrimitiveString>(str_obj->value.str);
      return JSValue(prim_string);
    }
    else {
      JSValue err = NativeFunctions::build_error_internal(vm, u"String.prototype.valueOf can only accept argument "
                                                               "of type string or string object.");
      return Completion::with_throw(err);
    }
  }

};

} // namespace njs

#endif //NJS_JS_STRING_PROTOTYPE_H
