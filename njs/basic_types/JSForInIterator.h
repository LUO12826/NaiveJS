#ifndef NJS_JS_FOR_IN_ITERATOR_H
#define NJS_JS_FOR_IN_ITERATOR_H

#include <vector>
#include "njs/include/robin_hood.h"
#include "JSObject.h"
#include "JSArray.h"
#include "JSString.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/AtomPool.h"

namespace njs {

using u32 = uint32_t;
using std::vector;
template <typename T>
using set = robin_hood::unordered_flat_set<T>;

class JSForInIterator : public JSObject {
 public:

  static JSValue build_for_object(NjsVM& vm, JSValue object) {
    auto *iter = vm.heap.new_object<JSForInIterator>(vm);
    if (not object.is_object()) return JSValue(iter);

    set<u32> visited;
    vector<u32> keys;
    JSObject *obj = object.as_object;

    while (true) {
      if (obj->is_array()) [[unlikely]] {
        auto *arr = obj->as<JSArray>();
        for (size_t i = 0; i < arr->get_dense_array().size(); i++) {
          if (arr->get_dense_array()[i].is_uninited()) [[unlikely]] continue;
          keys.push_back(vm.u32_to_atom(u32(i)));
        }
      } else if (obj->get_class() == CLS_STRING) [[unlikely]] {
        auto *str = obj->as<JSString>();
        size_t len = str->get_prim_value()->length();
        for (size_t i = 0; i < len; i++) {
          keys.push_back(vm.u32_to_atom(u32(i)));
        }
      }
      for (auto& [key, prop_desc] : obj->storage) {
        if (key.type == JSValue::SYMBOL) [[unlikely]] continue;
        if (visited.contains(key.atom)) [[unlikely]] continue;
        visited.insert(key.atom);
        if (not prop_desc.flag.enumerable) continue;
        keys.push_back(key.atom);
      }

      JSValue proto = obj->get_proto();
      if (proto.is_null()) break;
      obj = proto.as_object;
    }

    vm.heap.write_barrier(iter, object);
    iter->object = object;
    iter->collected_keys = std::move(keys);

    return JSValue(iter);
  }

  explicit JSForInIterator(NjsVM& vm) : JSObject(vm, CLS_FOR_IN_ITERATOR, vm.iterator_prototype) {}

  u16string_view get_class_name() override {
    return u"ForInIterator";
  }

  bool gc_scan_children(njs::GCHeap &heap) override {
    bool child_young = false;
    child_young |= JSObject::gc_scan_children(heap);
    if (object.needs_gc()) [[likely]] {
      child_young |= heap.gc_visit_object(object.as_GCObject);
    }
    return child_young;
  }

  void gc_mark_children() override {
    JSObject::gc_mark_children();
    if (object.needs_gc()) [[likely]] {
      gc_mark_object(object.as_GCObject);
    }
  }

  bool gc_has_young_child(GCObject *oldgen_start) override {
    if (JSObject::gc_has_young_child(oldgen_start)) return true;
    return object.needs_gc() && object.as_GCObject < oldgen_start;
  }

  JSValue next(NjsVM& vm) {
    if (index < collected_keys.size()) [[likely]] {
      u16string_view key_str = vm.atom_to_str(collected_keys[index]);
      index += 1;
      return vm.new_primitive_string(key_str);
    } else {
      return JSValue::uninited;
    }
  }

 private:
  u32 index {0};
  JSValue object;
  vector<u32> collected_keys;
};

}



#endif // NJS_JS_FOR_IN_ITERATOR_H
