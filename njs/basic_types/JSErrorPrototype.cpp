#include "JSErrorPrototype.h"
#include "njs/vm/NjsVM.h"

namespace njs {

JSErrorPrototype::JSErrorPrototype(NjsVM& vm, JSErrorType type)
: JSObject(ObjClass::CLS_ERROR_PROTO) {
  set_prop(vm, u"name", vm.new_primitive_string(native_error_name[type]));
  add_method(vm, u"toString", JSErrorPrototype::toString);
}

Completion JSErrorPrototype::toString(vm_func_This_args_flags) {
  return vm.new_primitive_string(u"(Fixme)");
}

}
