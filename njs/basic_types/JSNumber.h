#ifndef JS_NUMBER_H
#define JS_NUMBER_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSNumber : public JSObject {
  public:
  JSNumber(NjsVM& vm, double num) :
      JSObject(vm, CLS_NUMBER, vm.number_prototype),
      value(num) {}

  u16string_view get_class_name() override {
    return u"Number";
  }

  double value;
};

} // namespace njs

#endif //JS_NUMBER_H
