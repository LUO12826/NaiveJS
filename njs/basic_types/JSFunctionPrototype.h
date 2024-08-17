#ifndef NJS_JSFUNCTION_PROTOTYPE_H
#define NJS_JSFUNCTION_PROTOTYPE_H

#include "JSObject.h"
#include "JSBoundFunction.h"
#include "njs/vm/native.h"
#include "njs/common/Completion.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/common_def.h"

namespace njs {

class JSFunctionPrototype : public JSObject {
 public:
  JSFunctionPrototype(NjsVM& vm) : JSObject(CLS_FUNCTION_PROTO) {}

  // We need this to solve the chicken or egg question.
  // see also NjsVM::init_prototypes
  void add_methods(NjsVM& vm) {
    add_method(vm, u"call", JSFunctionPrototype::call);
    add_method(vm, u"bind", JSFunctionPrototype::bind);
    add_method(vm, u"apply", JSFunctionPrototype::apply);
  }

  u16string_view get_class_name() override {
    return u"FunctionPrototype";
  }

  static Completion call(vm_func_This_args_flags) {
    if (This.is_undefined()) [[unlikely]] return undefined;
    if (args.empty()) [[unlikely]] {
      return vm.call_internal(This, undefined, undefined, args, flags);
    } else {
      return vm.call_internal(This, args[0], undefined, args.subspan(1), flags);
    }
  }

  static Completion bind(vm_func_This_args_flags) {
    if (not This.is_function()) [[unlikely]] {
      return vm.throw_error(JS_TYPE_ERROR,
                            u"Function.prototype.apply can only be called on a function.");
    }
    auto *bound_func = vm.heap.new_object<JSBoundFunction>(vm, This);
    assert(args.size() > 0); // TODO
    bound_func->set_args(vm, args.subspan(1));

    if (This.as_object->is_direct_function()) [[likely]] {
      bound_func->set_this(vm, args[0]);
      bound_func->set_proto(vm, This.as_func->get_proto());
    } else if (object_class(This) == CLS_BOUND_FUNCTION) {
      bound_func->set_this(vm, This.as_Object<JSBoundFunction>()->get_this());
      bound_func->set_proto(vm, This.as_object->get_proto());
    } else {
      assert(false);
    }

    JSValue ret(JSValue::FUNCTION);
    ret.as_object = bound_func;
    return ret;
  }

  static Completion apply(vm_func_This_args_flags) {
    if (not This.is_function()) [[unlikely]] {
      return vm.build_error(JS_TYPE_ERROR,
                            u"Function.prototype.apply can only be called on a function.");
    }
    if (args.empty()) [[unlikely]] {
      return vm.call_internal(This, undefined, undefined, args, flags);
    } else if (args.size() == 1) [[unlikely]] {
      return vm.call_internal(This, args[0], undefined, args.subspan(1), flags);
    } else {
      if (not args[1].is_object()) [[unlikely]] {
        return vm.throw_error(JS_TYPE_ERROR, u"CreateListFromArrayLike called on non-object");
      }
      JSObject *arr = args[1].as_object;
      JSValue len = TRYCC(arr->get_property(vm, JSAtom(AtomPool::k_length)));
      assert(len.is_float64());
      size_t length = len.as_f64;
      vector<JSValue> argv(length);
      for (int i = 0; i < length; i++) {
        JSValue idx_atom = JSAtom(vm.atom_pool.atomize_u32(i));
        argv[i] = TRYCC(arr->get_property(vm, idx_atom));
      }
      return vm.call_internal(This, args[0], undefined, Span(argv.data(), length), flags);
    }
  }
};

}



#endif // NJS_JSFUNCTION_PROTOTYPE_H
