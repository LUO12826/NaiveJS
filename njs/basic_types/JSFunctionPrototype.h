#ifndef NJS_JSFUNCTION_PROTOTYPE_H
#define NJS_JSFUNCTION_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NativeFunction.h"

namespace njs {

class JSFunctionPrototype : public JSObject {
 public:
  JSFunctionPrototype(NjsVM& vm) : JSObject(ObjClass::CLS_FUNCTION_PROTO) {}

  // We need this to solve the chicken or egg question.
  // see also NjsVM::init_prototypes
  void add_methods(NjsVM& vm) {
    add_method(vm, u"valueOf", NativeFunction::clear_interval);
  }

  u16string_view get_class_name() override {
    return u"FunctionPrototype";
  }
};

}



#endif // NJS_JSFUNCTION_PROTOTYPE_H
