#ifndef JS_NUMBER_H
#define JS_NUMBER_H

#include "JSObject.h"

namespace njs {

class JSNumber : public JSObject {
  public:
  JSNumber(): JSObject(ObjectClass::CLS_NUMBER) {}

  explicit JSNumber(double num): JSObject(ObjectClass::CLS_NUMBER), number(num) {}

  JSNumber(NjsVM& vm, double num) :
      JSObject(ObjectClass::CLS_NUMBER, vm.number_prototype),
      number(num) {}

  u16string_view get_class_name() override {
    return u"Number";
  }

  double number;
};

} // namespace njs

#endif //JS_NUMBER_H
