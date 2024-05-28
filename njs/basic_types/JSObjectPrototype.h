#ifndef NJS_JSOBJECT_PROTOTYPE_H
#define NJS_JSOBJECT_PROTOTYPE_H


#include "JSFunction.h"
#include "JSObject.h"
#include "conversion.h"
#include <njs/vm/NjsVM.h>
#include "njs/vm/Completion.h"
#include "njs/common/common_def.h"

namespace njs {

class JSObjectPrototype : public JSObject {
 public:
  JSObjectPrototype(NjsVM& vm) : JSObject(ObjClass::CLS_OBJECT_PROTO) {
    add_method(vm, u"valueOf", JSObjectPrototype::valueOf);
    add_method(vm, u"toString", JSObjectPrototype::toString);
    add_method(vm, u"toLocaleString", JSObjectPrototype::toLocaleString);
    add_method(vm, u"hasOwnProperty", JSObjectPrototype::hasOwnProperty);
  }

  u16string_view get_class_name() override {
    return u"ObjectPrototype";
  }

  static Completion valueOf(vm_func_This_args_flags) {
    return This;
  }

  static Completion toString(vm_func_This_args_flags) {
    JSObject *obj;
    if (This.is_object()) [[likely]] {
      obj = This.as_object();
    } else {
      obj = TRY_COMP_COMP(js_to_object(vm, This)).as_object();
    }
    u16string res = u"[object ";
    res += obj->get_class_name();
    res += u"]";
    return vm.new_primitive_string(std::move(res));
  }

  static Completion toLocaleString(vm_func_This_args_flags) {
    // TODO
    return toString(JS_NATIVE_FUNC_ARGS);
  }

  static Completion hasOwnProperty(vm_func_This_args_flags) {
    assert(args.size() > 0);
    JSObject *obj = This.as_object();
    JSValue prop_name = args[0];
    Completion comp = js_to_property_key(vm, prop_name);
    if (comp.is_throw()) return comp;

    return obj->has_own_property(vm, comp.get_value());
  }

};

} // namespace njs

#endif // NJS_JSOBJECT_PROTOTYPE_H
