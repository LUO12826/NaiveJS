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
    JSObject *obj = object.as_object();

    while (true) {
      if (obj->get_class() == ObjClass::CLS_ARRAY) [[unlikely]] {
        auto *arr = obj->as<JSArray>();
        if (arr->is_fast_array) {
          for (size_t i = 0; i < arr->dense_array.size(); i++) {
            if (arr->dense_array[i].is_uninited()) [[unlikely]] continue;
            keys.push_back(vm.u32_to_atom(u32(i)));
          }
        }
      } else if (obj->get_class() == ObjClass::CLS_STRING) {
        auto *str = obj->as<JSString>();
        size_t len = str->value.length();
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
      obj = proto.as_object();
    }

    iter->object = object;
    iter->collected_keys = std::move(keys);

    return JSValue(iter);
  }

  JSForInIterator(NjsVM& vm) : JSObject(ObjClass::CLS_FOR_IN_ITERATOR, vm.iterator_prototype) {}

  u16string_view get_class_name() override {
    return u"ForInIterator";
  }

  JSValue next(NjsVM& vm) {
    if (index < collected_keys.size()) [[likely]] {
      u16string key_str = vm.atom_to_str(collected_keys[index]);
      index += 1;
      return vm.new_primitive_string(std::move(key_str));
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
