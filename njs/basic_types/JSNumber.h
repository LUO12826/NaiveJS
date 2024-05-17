#ifndef JS_NUMBER_H
#define JS_NUMBER_H

#include "JSObject.h"

namespace njs {

class JSNumber : public JSObject {
  public:
  JSNumber(NjsVM& vm, double num) :
      JSObject(ObjClass::CLS_NUMBER, vm.number_prototype),
      value(num) {}

  u16string_view get_class_name() override {
    return u"Number";
  }

  double value;
};

} // namespace njs

#endif //JS_NUMBER_H
