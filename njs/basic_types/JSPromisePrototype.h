#ifndef NJS_JS_PROMISE_PROTOTYPE_H
#define NJS_JS_PROMISE_PROTOTYPE_H

#include "JSObject.h"
#include "JSPromise.h"

namespace njs {

class JSPromisePrototype : public JSObject {
 public:
  explicit JSPromisePrototype(NjsVM &vm) : JSObject(CLS_PROMISE_PROTO) {
    add_method(vm, u"then", JSPromisePrototype::promise_then);
    add_method(vm, u"catch", JSPromisePrototype::promise_catch);
    add_method(vm, u"finally", JSPromisePrototype::promise_finally);
  }

  u16string_view get_class_name() override {
    return u"PromisePrototype";
  }

  static Completion promise_then(vm_func_This_args_flags) {
    assert(This.is_object() && This.as_object->get_class() == CLS_PROMISE);
    const JSValue& on_fulfilled = args.size() > 0 ? args[0] : undefined;
    const JSValue& on_rejected = args.size() > 1 ? args[1] : undefined;

    return This.as_Object<JSPromise>()->then(vm, on_fulfilled, on_rejected);
  }

  static Completion promise_catch(vm_func_This_args_flags) {
    assert(This.is_object() && This.as_object->get_class() == CLS_PROMISE);
    const JSValue& on_rejected = args.size() > 0 ? args[0] : undefined;

    return This.as_Object<JSPromise>()->then(vm, undefined, on_rejected);
  }

  static Completion promise_finally(vm_func_This_args_flags) {
    assert(This.is_object() && This.as_object->get_class() == CLS_PROMISE);
    const JSValue& on_finally = args.size() > 0 ? args[0] : undefined;

    return This.as_Object<JSPromise>()->finally(vm, on_finally);
  }
};

}

#endif // NJS_JS_PROMISE_PROTOTYPE_H
