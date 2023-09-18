#ifndef NJS_JS_STRING_PROTOTYPE_H
#define NJS_JS_STRING_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/ArrayRef.h"
#include "JSFunction.h"
#include "JSString.h"

namespace njs {

class JSStringPrototype : public JSObject {
 public:
  JSStringPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_STRING_PROTO) {
    add_method(vm, u"charAt", JSStringPrototype::char_at);
  }

  static JSValue char_at(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    assert(args.size() > 0 && args[0].tag_is(JSValue::NUM_FLOAT));
    assert(func.This.tag_is(JSValue::STRING) || func.This.tag_is(JSValue::STRING_OBJ));

    u16string& str = func.This.tag_is(JSValue::STRING) ? func.This.val.as_primitive_string->str
                                                       : func.This.val.as_string->value.str;

    double index = args[0].val.as_float64;
    if (index < 0 || index > str.size()) return JSValue(new PrimitiveString(u""));
    return JSValue(new PrimitiveString(u16string{str[(size_t)index]}));
  }

  u16string_view get_class_name() override {
    return u"StringPrototype";
  }

};

} // namespace njs

#endif //NJS_JS_STRING_PROTOTYPE_H
