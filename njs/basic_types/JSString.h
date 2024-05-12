#ifndef NJS_JSSTRING_H
#define NJS_JSSTRING_H

#include "JSObject.h"
#include "PrimitiveString.h"

namespace njs {

class JSString : public JSObject {
 public:
  JSString(): JSObject(ObjClass::CLS_STRING) {}

  explicit JSString(const PrimitiveString& str): JSObject(ObjClass::CLS_STRING), value(str.str) {}

  JSString(NjsVM& vm, const PrimitiveString& str) :
    JSObject(ObjClass::CLS_STRING, vm.string_prototype),
    value(str.str) {}

  u16string_view get_class_name() override {
    return u"String";
  }

//  std::string description() override;
//  std::string to_string(NjsVM& vm) override;
//  void to_json(u16string& output, NjsVM& vm) const override;

  PrimitiveString value;
};

} // namespace njs

#endif //NJS_JSSTRING_H
