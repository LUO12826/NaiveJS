#ifndef NJS_JSDATE_H
#define NJS_JSDATE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"

namespace njs {

class JSDate : public JSObject {
 public:
  JSDate(NjsVM& vm) : JSObject(ObjClass::CLS_DATE, vm.date_prototype) {}

  u16string_view get_class_name() override {
    return u"Date";
  }

};

} // namespace njs

#endif // NJS_JSDATE_H
