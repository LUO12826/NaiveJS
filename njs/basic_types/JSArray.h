#ifndef NJS_JSARRAY_H
#define NJS_JSARRAY_H

#include <vector>
#include <cmath>
#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/AtomPool.h"
#include "njs/basic_types/atom.h"
#include "njs/basic_types/conversion.h"

namespace njs {

using u32 = uint32_t;

class JSArray: public JSObject {
 public:
  JSArray(): JSArray(0) {}

  explicit JSArray(int length): JSObject(ObjClass::CLS_ARRAY) {
    add_prop_trivial(AtomPool::ATOM_length, JSValue((double)length));
  }

  JSArray(NjsVM& vm, int length): JSObject(ObjClass::CLS_ARRAY, vm.array_prototype) {
    add_prop_trivial(AtomPool::ATOM_length, JSValue((double)length));
  }

  void gc_scan_children(GCHeap& heap) override;

  u16string_view get_class_name() override {
    return u"Array";
  }
  std::string description() override;
  std::string to_string(NjsVM& vm) const override;
  void to_json(u16string& output, NjsVM& vm) const override;

  JSValue get_element_fast(u32 index) {
    if (index < dense_array.size()) return dense_array[index];
    return undefined;
  }

  void set_element_fast(u32 index, JSValue val) {
    if (index < dense_array.size()) {
      dense_array[index] = val;
    } else {
      auto old_size = dense_array.size();
      dense_array.resize((size_t)std::ceil(index * 1.5));
      auto new_size = dense_array.size();
      for (size_t i = old_size; i < index; i++) {
        dense_array[i].set_uninited();
      }
      for (size_t i = index + 1; i < new_size; i++) {
        dense_array[i].set_uninited();
      }
      dense_array[index] = val;
    }
  }

  Completion get_element(NjsVM& vm, JSValue key) {
    // The float value can be interpreted as array index
    if (key.is_float64() && key.is_non_negative() && key.is_integer()) {
      auto int_idx = int64_t(key.as_f64());
      if (int_idx < UINT32_MAX) {
        return get_element_fast(int_idx);
      } else {
        return get_prop(vm, to_u16string(std::to_string(int_idx)));
      }
    }
    // in this case, the float value is interpreted as an ordinary property key
    else if (key.is_float64()) {
      // TODO: change this to the real `ToString`
      u16string num_str = to_u16string(std::to_string(key.val.as_f64));
      return get_prop(vm, num_str);
    }
    else if (key.is_atom() || key.is_prim_string()) {
      u32 atom;
      if (key.is_atom()) {
        atom = key.val.as_atom;
      } else {
        atom = vm.str_to_atom(key.val.as_prim_string->str);
      }
      if (atom_is_int(atom)) {
        return get_element_fast(atom_get_int(atom));
      } else {
        return get_prop(vm, atom);
      }
    }
    else {
      // TODO: ToString part
      assert(false);
    }
  }

  Completion set_element(NjsVM& vm, JSValue key, JSValue val) {
#define RET_ERR_OR_UNDEF { if (res.is_error()) { return Completion::with_throw(res.get_error()); } return undefined; }
    // The float value can be interpreted as array index
    if (key.is_atom() || key.is_prim_string()) {
      u32 atom;
      if (key.is_atom()) {
        atom = key.val.as_atom;
      } else {
        atom = vm.str_to_atom(key.val.as_prim_string->str);
      }
      if (atom_is_int(atom)) {
        set_element_fast(atom_get_int(atom), val);
        return undefined;
      } else {
        auto res = set_prop(vm, atom, val);
        RET_ERR_OR_UNDEF
      }
    }
    else if (key.is_float64() && key.is_non_negative() && key.is_integer()) {
      auto int_idx = int64_t(key.as_f64());
      if (int_idx < UINT32_MAX) {
        set_element_fast(int_idx, val);
        return undefined;
      } else {
        auto res = set_prop(vm, to_u16string(std::to_string(int_idx)), val);
        RET_ERR_OR_UNDEF
      }
    }
    // in this case, the float value is interpreted as an ordinary property key
    else if (key.is_float64()) {
      // TODO: change this to the real `ToString`
      u16string num_str = to_u16string(std::to_string(key.val.as_f64));
      auto res = set_prop(vm, num_str, val);
      RET_ERR_OR_UNDEF
    }
    else {
      // TODO: ToString part
      assert(false);
    }
  }

  u32 get_length() {
    assert(has_own_property(AtomPool::ATOM_length));
    return (u32)get_prop_trivial(AtomPool::ATOM_length).val.as_f64;
  }

  void set_length(u32 length) {
    add_prop_trivial(AtomPool::ATOM_length, JSValue(double(length)));
  }

  std::vector<JSValue> dense_array;
  bool is_fast_array {true};
};

}

#endif // NJS_JSARRAY_H
