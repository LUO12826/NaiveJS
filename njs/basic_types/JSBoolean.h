#ifndef JS_BOOLEAN_H
#define JS_BOOLEAN_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSBoolean : public JSObject {
 public:
  JSBoolean(): JSObject(ObjectClass::CLS_BOOLEAN) {}

  explicit JSBoolean(bool b): JSObject(ObjectClass::CLS_BOOLEAN), val(b) {}

  JSBoolean(NjsVM& vm, bool b) :
      JSObject(ObjectClass::CLS_BOOLEAN, vm.boolean_prototype),
      val(b) {}

  u16string_view get_class_name() override {
    return u"Boolean";
  }

  bool val;
};

} // namespace njs

#endif //JS_BOOLEAN_H
