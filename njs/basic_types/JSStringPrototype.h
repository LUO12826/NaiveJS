#ifndef NJS_JS_STRING_PROTOTYPE_H
#define NJS_JS_STRING_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/ArrayRef.h"
#include "JSFunction.h"
#include "JSString.h"
#include "njs/vm/Completion.h"

namespace njs {

class JSStringPrototype : public JSObject {
 public:
  explicit JSStringPrototype(NjsVM &vm) : JSObject(ObjectClass::CLS_STRING_PROTO) {
    add_method(vm, u"charAt", JSStringPrototype::char_at);
  }

  u16string_view get_class_name() override {
    return u"StringPrototype";
  }

  static Completion char_at(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    assert(func.This.is(JSValue::STRING) || func.This.is(JSValue::STRING_OBJ));

    u16string& str = func.This.is(JSValue::STRING) ? func.This.val.as_primitive_string->str
                                                   : func.This.val.as_string->value.str;

    double index = args[0].val.as_float64;
    if (index < 0 || index > str.size()) return JSValue(new PrimitiveString(u""));
    return JSValue(new PrimitiveString(u16string{str[(size_t)index]}));
  }

};

} // namespace njs

#endif //NJS_JS_STRING_PROTOTYPE_H
