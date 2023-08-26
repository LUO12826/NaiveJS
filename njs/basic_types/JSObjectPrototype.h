#ifndef NJS_JSOBJECT_PROTOTYPE_H
#define NJS_JSOBJECT_PROTOTYPE_H

#include "JSObject.h"

namespace njs {

class JSObjectPrototype : public JSObject {
 public:
  JSObjectPrototype() : JSObject(ObjectClass::CLS_OBJECT_PROTO, JSValue::null) {}

  u16string_view get_class_name() override {
    return u"ObjectPrototype";
  }

};

} // namespace njs

#endif // NJS_JSOBJECT_PROTOTYPE_H
