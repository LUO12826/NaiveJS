#ifndef NJS_JS_ARRAY_ITERATOR_H
#define NJS_JS_ARRAY_ITERATOR_H

#include <vector>
#include "JSObject.h"
#include "JSArray.h"
#include "JSString.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/AtomPool.h"
#include "njs/common/common_def.h"
#include "JSIteratorPrototype.h"

namespace njs {

using u32 = uint32_t;
using std::vector;

class JSArrayIterator : public JSObject {

  static Completion iter_next(vm_func_This_args_flags) {
    assert(This.is_object());
    assert(object_class(This) == CLS_ARRAY_ITERATOR);

    auto *iter = This.as_Object<JSArrayIterator>();
    return iter->next(vm);
  }

 public:
  JSArrayIterator(NjsVM& vm, JSValue array, JSIteratorKind kind)
    : JSObject(CLS_ARRAY_ITERATOR, vm.iterator_prototype)
    , array(array), kind(kind) {
    add_method(vm, u"next", JSArrayIterator::iter_next);
  }

  u16string_view get_class_name() override {
    return u"ArrayIterator";
  }

  void gc_scan_children(njs::GCHeap &heap) override {
    JSObject::gc_scan_children(heap);
    heap.gc_visit_object(array, array.as_GCObject);
  }

  Completion next(NjsVM& vm) {
    JSArray& arr = *array.as_Object<JSArray>();
    JSValue value;
    bool done;

    if (index < arr.get_length()) [[likely]] {
      done = false;
      if (kind == JSIteratorKind::VALUE) [[likely]] {
        JSValue idx_atom = JSAtom(vm.u32_to_atom(index));
        value = TRYCC(arr.get_property_impl(vm, idx_atom));
      }
      else if (kind == JSIteratorKind::KEY) {
        value = JSFloat(index);
      }
      else {
        JSArray& tmp_arr = *vm.heap.new_object<JSArray>(vm, 2);

        JSValue idx_atom = JSAtom(vm.u32_to_atom(index));
        tmp_arr.dense_array[0] = JSFloat(index);
        tmp_arr.dense_array[1] = TRYCC(arr.get_property_impl(vm, idx_atom));
        value = JSValue(&tmp_arr);
      }
      index += 1;

    } else {
      done = true;
    }
    JSObject *res = vm.new_object();
    res->add_prop_trivial(AtomPool::k_done, JSValue(done), PFlag::VECW);
    res->add_prop_trivial(AtomPool::k_value, value, PFlag::VECW);

    return JSValue(res);
  }

 private:
  u32 index {0};
  JSValue array;
  JSIteratorKind kind;
};

}



#endif // NJS_JS_ARRAY_ITERATOR_H
