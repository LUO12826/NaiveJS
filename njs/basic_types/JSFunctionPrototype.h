#ifndef NJS_JSFUNCTION_PROTOTYPE_H
#define NJS_JSFUNCTION_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NativeFunction.h"
#include "njs/vm/Completion.h"
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
    add_method(vm, u"apply", JSFunctionPrototype::apply);
  }

  u16string_view get_class_name() override {
    return u"FunctionPrototype";
  }

  static Completion call(vm_func_This_args_flags) {
    if (args.size() == 0) [[unlikely]] {
      return vm.call_internal(This.val.as_func, undefined, nullptr, args, flags);
    } else {
      return vm.call_internal(This.val.as_func, args[0], nullptr, args.subarray(1), flags);
    }
  }

  static Completion apply(vm_func_This_args_flags) {
    if (args.size() == 0) [[unlikely]] {
      return vm.call_internal(This.val.as_func, undefined, nullptr, args, flags);
    } else if (args.size() == 1) [[unlikely]] {
      return vm.call_internal(This.val.as_func, args[0], nullptr, args.subarray(1), flags);
    } else {
      if (not args[1].is_object()) [[unlikely]] {
        return CompThrow(vm.build_error_internal(
            JS_TYPE_ERROR, u"CreateListFromArrayLike called on non-object"));
      }
      JSObject *arr = args[1].as_object();
      JSValue len = TRY_COMP(arr->get_property(vm, JSAtom(AtomPool::k_length)));
      assert(len.is_float64());
      size_t length = len.val.as_f64;
      vector<JSValue> argv(length);
      for (int i = 0; i < length; i++) {
        JSValue idx_atom = JSAtom(vm.atom_pool.atomize_u32(i));
        argv[i] = TRY_COMP(arr->get_property(vm, idx_atom));
      }
      return vm.call_internal(This.val.as_func, args[0], nullptr,
                              ArrayRef(argv.data(), length), flags);
    }
  }
};

}



#endif // NJS_JSFUNCTION_PROTOTYPE_H
