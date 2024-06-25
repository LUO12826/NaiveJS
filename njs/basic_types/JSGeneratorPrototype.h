#ifndef NJS_JS_GENERATOR_PROTOTYPE_H
#define NJS_JS_GENERATOR_PROTOTYPE_H

#include "JSObject.h"
#include "JSGenerator.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSGeneratorPrototype : public JSObject {
 public:
  explicit JSGeneratorPrototype(NjsVM &vm)
      : JSObject(vm, CLS_GENERATOR_PROTO, vm.object_prototype) {
    add_symbol_method(vm, AtomPool::k_sym_iterator,  JSGeneratorPrototype::get_iter);
    add_method(vm, u"next", JSGeneratorPrototype::next_);
    add_method(vm, u"return", JSGeneratorPrototype::return_);
    add_method(vm, u"throw", JSGeneratorPrototype::throw_);
  }

  u16string_view get_class_name() override {
    return u"GeneratorPrototype";
  }

  static Completion get_iter(vm_func_This_args_flags) {
    return This;
  }

  static Completion next_(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_GENERATOR) [[likely]] {
      auto *gen = This.as_Object<JSGenerator>();
      if (not gen->done) {
        JSValueRef arg = args.empty() ? undefined : args[0];
        gen->exec_state->stack_frame.sp[0] = arg;
      }
      return vm.generator_resume(This, gen->exec_state);

    } else {
      return vm.build_error(JS_TYPE_ERROR,
                            u"Generator.prototype.next can only be called by generator object");
    }
  }

  static Completion return_(vm_func_This_args_flags) {
    assert(false);
    return undefined;
  }

  static Completion throw_(vm_func_This_args_flags) {
    assert(false);
    return undefined;
  }

};

}

#endif // NJS_JS_GENERATOR_PROTOTYPE_H
