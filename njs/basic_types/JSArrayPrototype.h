#ifndef NJS_JSARRAY_PROTOTYPE_H
#define NJS_JSARRAY_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/ArrayRef.h"
#include "JSFunction.h"

namespace njs {

class NjsVM;

class JSArrayPrototype : public JSObject {
 public:
  JSArrayPrototype(NjsVM& vm) : JSObject(ObjectClass::CLS_ARRAY_PROTO) {
    add_method(vm, u"at", JSArrayPrototype::at);
    add_method(vm, u"push", JSArrayPrototype::push);
  }

  u16string_view get_class_name() override {
    return u"ArrayPrototype";
  }

  static JSValue at(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    assert(args.size() > 0 && args[0].tag_is(JSValue::NUM_FLOAT));
    assert(func.This.tag_is(JSValue::ARRAY));

    JSArray *array = func.This.val.as_array;
    double index = args[0].val.as_float64;
    if (index < 0 || index > array->get_length()) return JSValue::undefined;
    return array->access_element((u32)index, false);
  }

  static JSValue push(NjsVM& vm, JSFunction& func, ArrayRef<JSValue> args) {
    assert(func.This.tag_is(JSValue::ARRAY));
    JSArray *array = func.This.val.as_array;

    size_t old_size = array->dense_array.size();
    size_t new_size = old_size + args.size();

    array->dense_array.resize(new_size);

    for (size_t i = 0; i < args.size(); i++) {
      array->dense_array[old_size + i].assign(args[i]);
    }

    array->set_length(u32(new_size));

    // return the length after push
    return JSValue(double(new_size));
  }
};

} // njs

#endif //NJS_JSARRAY_PROTOTYPE_H
