#ifndef NJS_JS_NUMBER_PROTOTYPE_H
#define NJS_JS_NUMBER_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSNumberPrototype : public JSObject {
 public:
  explicit JSNumberPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_NUMBER_PROTO) {
  }

  u16string_view get_class_name() override {
    return u"NumberPrototype";
  }

};

} // namespace njs

#endif //NJS_JS_NUMBER_PROTOTYPE_H
