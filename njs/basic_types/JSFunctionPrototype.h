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
  JSFunctionPrototype(NjsVM& vm) : JSObject(ObjClass::CLS_FUNCTION_PROTO) {}

  // We need this to solve the chicken or egg question.
  // see also NjsVM::init_prototypes
  void add_methods(NjsVM& vm) {
    add_method(vm, u"call", JSFunctionPrototype::call);
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
};

}



#endif // NJS_JSFUNCTION_PROTOTYPE_H
