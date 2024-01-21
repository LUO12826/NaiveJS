#ifndef NJS_JSOBJECT_PROTOTYPE_H
#define NJS_JSOBJECT_PROTOTYPE_H

#include <njs/vm/NjsVM.h>

#include "JSFunction.h"
#include "JSObject.h"
#include "njs/vm/Completion.h"

namespace njs {

class JSObjectPrototype : public JSObject {
 public:
  JSObjectPrototype(NjsVM& vm) : JSObject(ObjectClass::CLS_OBJECT_PROTO, JSValue::null) {
    add_method(vm, u"valueOf", JSObjectPrototype::valueOf);
    add_method(vm, u"toString", JSObjectPrototype::toString);
    add_method(vm, u"toLocaleString", JSObjectPrototype::toLocaleString);
    add_method(vm, u"hasOwnProperty", JSObjectPrototype::hasOwnProperty);
  }

  u16string_view get_class_name() override {
    return u"ObjectPrototype";
  }

  static Completion valueOf(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    return func.This;
  }

  static Completion toString(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    return JSValue(new PrimitiveString(u"[object Object]"));
  }

  static Completion toLocaleString(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    // TODO
    return JSValue(new PrimitiveString(u"[object Object]"));
  }

  static Completion hasOwnProperty(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    JSObject *obj = func.This.as_object();
    JSValue prop_name = args[0];
    assert(prop_name.is_primitive_string());

    bool has = vm.str_pool.has_string(prop_name.val.as_primitive_string->str);
    if (!has) return JSValue(false);

    u32 str_atom = vm.str_pool.add_string(prop_name.val.as_primitive_string->str);
    has = obj->has_own_property(str_atom);

    return JSValue(has);
  }

};

} // namespace njs

#endif // NJS_JSOBJECT_PROTOTYPE_H
