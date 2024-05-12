#ifndef JS_BOOLEAN_H
#define JS_BOOLEAN_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSBoolean : public JSObject {
 public:
  JSBoolean(): JSObject(ObjClass::CLS_BOOLEAN) {}

  explicit JSBoolean(bool b): JSObject(ObjClass::CLS_BOOLEAN), value(b) {}

  JSBoolean(NjsVM& vm, bool b) :
      JSObject(ObjClass::CLS_BOOLEAN, vm.boolean_prototype),
      value(b) {}

  u16string_view get_class_name() override {
    return u"Boolean";
  }

  bool value;
};

} // namespace njs

#endif //JS_BOOLEAN_H
