#ifndef NJS_JSITERATOR_PROTOTYPE_H
#define NJS_JSITERATOR_PROTOTYPE_H

#include "JSObject.h"
#include "njs/common/AtomPool.h"
#include "njs/vm/NativeFunction.h"

namespace njs {

class JSIteratorPrototype : public JSObject {

  static Completion sym_itertor(vm_func_This_args_flags) {
    return This;
  }

 public:
  JSIteratorPrototype(NjsVM& vm) : JSObject(ObjClass::CLS_ITERATOR_PROTO) {
    add_symbol_method(vm, AtomPool::k_sym_iterator, JSIteratorPrototype::sym_itertor);
  }

  u16string_view get_class_name() override {
    return u"IteratorPrototype";
  }

};

}



#endif // NJS_JSITERATOR_PROTOTYPE_H
