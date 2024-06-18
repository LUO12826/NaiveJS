#ifndef NJS_JSDATE_PROTOTYPE_H
#define NJS_JSDATE_PROTOTYPE_H

#include "JSObject.h"
#include "JSDate.h"
#include "njs/common/Completion.h"
#include "njs/common/common_def.h"

namespace njs {

class NjsVM;

class JSDatePrototype : public JSObject {
 public:
  explicit JSDatePrototype(NjsVM& vm) {
    add_method(vm, u"valueOf", JSDatePrototype::valueOf);
    add_method(vm, u"getTime", JSDatePrototype::valueOf);
    add_method(vm, u"toString", JSDatePrototype::toString);
    add_method(vm, u"toJSON", JSDatePrototype::toJSON);
  }

  u16string_view get_class_name() override {
    return u"DatePrototype";
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_DATE) {
      double ts = This.as_Object<JSDate>()->timestamp;
      return JSValue(ts);
    } else {
      return vm.throw_error(JS_TYPE_ERROR, u"JSDate.prototype.valueOf called on non-date object");
    }
  }

  static Completion toString(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_DATE) {
      double ts = This.as_Object<JSDate>()->timestamp;
      return vm.new_primitive_string(get_date_string(ts, 0x13));
    } else {
      return vm.throw_error(JS_TYPE_ERROR, u"JSDate.prototype.toString called on non-date object");
    }
  }

  static Completion toJSON(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_DATE) {
      double ts = This.as_Object<JSDate>()->timestamp;
      return vm.new_primitive_string(get_date_string(ts, 0x23));
    } else {
      return vm.throw_error(JS_TYPE_ERROR, u"JSDate.prototype.toJSON called on non-date object");
    }
  }
};

}

#endif // NJS_JSDATE_PROTOTYPE_H
