#ifndef NJS_JSARRAY_H
#define NJS_JSARRAY_H

#include <vector>
#include <cmath>
#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/AtomPool.h"
#include "njs/utils/macros.h"
#include "njs/basic_types/atom.h"
#include "njs/basic_types/conversion.h"

namespace njs {

using u32 = uint32_t;

class JSArray: public JSObject {
 public:
  JSArray(NjsVM& vm, u32 length): JSObject(CLS_ARRAY, vm.array_prototype) {
    PFlag flag {
        .writable = true,
        .has_value = true,
    };
    add_prop_trivial(AtomPool::k_length, JSValue((double)length), flag);
    dense_array.resize(length);
    for (auto& ele : dense_array) {
      ele.set_uninited();
    }
  }

  void gc_scan_children(GCHeap& heap) override;

  u16string_view get_class_name() override {
    return u"Array";
  }
  std::string description() override;
  std::string to_string(NjsVM& vm) const override;
  void to_json(u16string& output, NjsVM& vm) const override;

  Completion get_property_impl(NjsVM& vm, JSValue key) override {
    JSValue res = TRY_COMP(get_index_or_atom(vm, key));

    if (res.is(JSValue::NUM_UINT32)) {
      return get_element_fast(res.val.as_u32);
    } else {
      assert(res.is_atom() || res.is_symbol());
      u32 atom = res.val.as_atom;
      if (atom_is_int(atom)) {
        return get_element_fast(atom_get_int(atom));
      } else {
        return get_prop(vm, res);
      }
    }
  }

  ErrorOr<bool> set_property_impl(NjsVM& vm, JSValue key, JSValue val) override {
    JSValue idx = TRY_ERR(get_index_or_atom(vm, key));

    if (idx.is(JSValue::NUM_UINT32)) {
      set_element_fast(idx.val.as_u32, val);
      return true;
    } else {
      assert(idx.is_atom() || idx.is_symbol());
      u32 atom = idx.val.as_atom;
      if (atom_is_int(atom)) {
        set_element_fast(atom_get_int(atom), val);
        return true;
      }
      else {
        if (atom == AtomPool::k_length) {
          double len = TRY_ERR(js_to_number(vm, val));
          int64_t len_int = len;
          if (len >= 0 && (double)len_int == len && len_int <= UINT32_MAX) {
            size_t old_len = dense_array.size();
            dense_array.resize(len_int);
            if (old_len < len_int) {
              for (size_t i = old_len; i < len_int; i++) {
                dense_array[i].set_uninited();
              }
            }

            set_length(len);
            return true;
          } else {
            return vm.build_error_internal(JS_RANGE_ERROR, u"Invalid array length");
          }
        }
        return set_prop(vm, idx, val);
      }
    }
  }

  // TODO: avoid key conversion at every level
  Completion has_property(NjsVM &vm, JSValue key) override {
    JSValue res = TRYCC(has_own_property(vm, key));
    assert(res.is_bool());
    if (res.val.as_bool) { // found
      return res;
    } else if (not _proto_.is_null()) {
      return _proto_.as_object()->has_property(vm, key);
    } else {
      return JSValue(false);
    }
  }

  Completion has_own_property(NjsVM& vm, JSValue key) override {
    auto has_fast_element = [this] (u32 index) {
      if (index < dense_array.size()) {
        JSValue val = dense_array[index];
        return JSValue(!val.is_uninited());
      }
      return JSValue(false);
    };

    JSValue k = TRYCC(get_index_or_atom(vm, key));

    if (k.is(JSValue::NUM_UINT32)) {
      return has_fast_element(k.val.as_u32);
    } else {
      assert(k.is_atom() || k.is_symbol());
      u32 atom = k.val.as_atom;
      if (atom_is_int(atom)) {
        return has_fast_element(atom_get_int(atom));
      } else {
        return JSValue(JSObject::has_own_property_atom(k));
      }
    }
  }

  JSValue get_element_fast(u32 index) {
    if (index < dense_array.size()) {
      JSValue val = dense_array[index];
      return val.is_uninited() ? prop_not_found : val;
    }
    return prop_not_found;
  }

  void set_element_fast(u32 index, JSValue val) {
    if (index < dense_array.size()) {
      dense_array[index] = val;
    } else {

      // since sparse arrays are not implemented, an assertion failure is
      // raised if the array is too large.
      if (index > 100000 && index > dense_array.size() * 100) [[unlikely]] {
        assert(false);
        fprintf(stderr, "very sparse array detected\n");
        exit(EXIT_FAILURE);
      }

      auto old_size = dense_array.size();
      dense_array.resize(index + 1);
      for (size_t i = old_size; i < index; i++) {
        dense_array[i].set_uninited();
      }
      dense_array[index] = val;
      set_length(index + 1);
    }
  }

  Completion get_index_or_atom(NjsVM& vm, JSValue key) {
    // The float value can be interpreted as array index
    if (key.is_float64()) {
      if (key.is_non_negative() && key.is_integer()) {
        auto int_idx = int64_t(key.val.as_f64);
        if (int_idx < UINT32_MAX) {
          return JSValue::U32(int_idx);
        } else {
          u32 atom = vm.str_to_atom_no_uint(to_u16string(int_idx));
          return JSAtom(atom);
        }
      }
      // in this case, the float value is interpreted as an ordinary property key
      else {
        u16string num_str = double_to_u16string(key.val.as_f64);
        u32 atom = vm.str_to_atom_no_uint(num_str);
        return JSAtom(atom);
      }

    } else {
      return js_to_property_key(vm, key);
    }
  }

  size_t push(ArrayRef<JSValue> values) {
    for (size_t i = 0; i < values.size(); i++) {
      dense_array.push_back(values[i]);
    }
    update_length();
    return dense_array.size();
  }

  JSValue pop() {
    if (dense_array.empty()) [[unlikely]] {
      return undefined;
    } else {
      JSValue last = dense_array.back();
      dense_array.pop_back();
      update_length();

      return unlikely(last.is_uninited()) ? prop_not_found : last;
    }
  }

  JSValue shift() {
    if (dense_array.empty()) [[unlikely]] {
      return undefined;
    } else {
      JSValue front = dense_array.front();
      dense_array.erase(dense_array.begin());
      update_length();

      return unlikely(front.is_uninited()) ? prop_not_found : front;
    }
  }


  u32 get_length() {
    assert(has_own_property_atom(AtomPool::k_length));
    return (u32)get_prop_trivial(AtomPool::k_length).val.as_f64;
  }

  void set_length(double len) {
    storage[JSObjectKey(AtomPool::k_length)].data.value.val.as_f64 = len;
  }

  void update_length() {
    storage[JSObjectKey(AtomPool::k_length)].data.value.val.as_f64 = dense_array.size();
  }

  std::vector<JSValue> dense_array;
  bool is_fast_array {true};
};

}

#endif // NJS_JSARRAY_H
