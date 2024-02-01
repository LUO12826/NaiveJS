#ifndef NJS_JS_BOOLEAN_PROTOTYPE_H
#define NJS_JS_BOOLEAN_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSBooleanPrototype : public JSObject {
 public:
  explicit JSBooleanPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_NUMBER_PROTO) {
  }

  u16string_view get_class_name() override {
    return u"BooleanPrototype";
  }

};

} // namespace njs

#endif //NJS_JS_BOOLEAN_PROTOTYPE_H
