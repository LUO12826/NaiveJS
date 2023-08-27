#ifndef NJS_JSFUNCTION_PROTOTYPE_H
#define NJS_JSFUNCTION_PROTOTYPE_H

#include "JSObject.h"

namespace njs {

class JSFunctionPrototype : public JSObject {
 public:
  JSFunctionPrototype() : JSObject(ObjectClass::CLS_FUNCTION_PROTO) {}

  u16string_view get_class_name() override {
    return u"FunctionPrototype";
  }
};

}



#endif //NJS_JSFUNCTION_PROTOTYPE_H
