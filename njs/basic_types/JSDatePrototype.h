#ifndef NJS_JSDATE_PROTOTYPE_H
#define NJS_JSDATE_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/Completion.h"
#include "njs/common/common_def.h"

namespace njs {

class NjsVM;

class JSDatePrototype : public JSObject {
 public:
  explicit JSDatePrototype(NjsVM& vm) {
    add_method(vm, u"valueOf", JSDatePrototype::valueOf);
    add_method(vm, u"toString", JSDatePrototype::toString);
  }

  u16string_view get_class_name() override {
    return u"DatePrototype";
  }

  static Completion valueOf(vm_func_This_args_flags) {

  }

  static Completion toString(vm_func_This_args_flags) {

  }
};

}

#endif // NJS_JSDATE_PROTOTYPE_H
