#ifndef NJS_JSARRAY_PROTOTYPE_H
#define NJS_JSARRAY_PROTOTYPE_H

#include "JSObject.h"
#include "JSObjectPrototype.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/ArrayRef.h"
#include "njs/common/common_def.h"
#include "JSFunction.h"
#include "njs/common/Completion.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSArrayIterator.h"
#include "njs/basic_types/conversion.h"

namespace njs {

class NjsVM;

class JSArrayPrototype : public JSObject {
 public:
  JSArrayPrototype(NjsVM& vm) : JSObject(CLS_ARRAY_PROTO) {
    add_symbol_method(vm, AtomPool::k_sym_iterator,  JSArrayPrototype::get_iter);
    add_method(vm, u"at", JSArrayPrototype::at);
    add_method(vm, u"push", JSArrayPrototype::push);
    add_method(vm, u"pop", JSArrayPrototype::pop);
    add_method(vm, u"shift", JSArrayPrototype::shift);
    add_method(vm, u"sort", JSArrayPrototype::sort);
    add_method(vm, u"toString", JSArrayPrototype::toString);
    add_method(vm, u"concat", JSArrayPrototype::concat);
    add_method(vm, u"join", JSArrayPrototype::join);
    add_method(vm, u"slice", JSArrayPrototype::slice);
    add_method(vm, u"splice", JSArrayPrototype::splice);
  }

  u16string_view get_class_name() override {
    return u"ArrayPrototype";
  }

  static Completion toString(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_ARRAY) [[likely]] {
      auto *arr = This.as_Object<JSArray>();
      u16string output;
      bool first = true;

      for (auto& val : arr->get_dense_array()) {
        if (first) first = false;
        else output += u',';

        if (not val.is_nil()) [[likely]] {
          JSValue s = TRYCC(js_to_string(vm, val));
          output += s.as_prim_string->view();
        }
      }
      return vm.new_primitive_string(output);

    } else {
      return JSObjectPrototype::toString(JS_NATIVE_FUNC_ARGS);
    }
  }

  static Completion join(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));

    u16string delimiter = u",";
    if (args.size() != 0 && !args[0].is_undefined()) [[likely]] {
      JSValue deli = TRYCC(js_to_string(vm, args[0]));
      delimiter = deli.as_prim_string->view();
    }

    auto *arr = This.as_Object<JSArray>();
    u16string output;
    bool first = true;

    for (auto& val : arr->get_dense_array()) {
      if (first) first = false;
      else output += delimiter;

      if (not val.is_nil()) [[likely]] {
        JSValue s = TRYCC(js_to_string(vm, val));
        output += s.as_prim_string->view();
      }
    }
    return vm.new_primitive_string(output);
  }

  static Completion get_iter(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    assert(This.is_object());
    assert(object_class(This) == CLS_ARRAY);

    auto *iter = vm.heap.new_object<JSArrayIterator>(vm, This, JSIteratorKind::VALUE);
    return JSValue(iter);
  }

  static Completion at(vm_func_This_args_flags) {
    assert(args.size() > 0);
    assert(This.is(JSValue::ARRAY));

    JSArray *array = This.as_array;
    return array->get_property_impl(vm, args[0]);
  }

  static Completion sort(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    auto& data_array = This.as_array->get_dense_array();
    Completion comp;
    // no compare function provided. Convert values to strings and do string sorting.
    if (args.size() == 0) {
      try {
        std::sort(data_array.begin(), data_array.end(), [&vm, &comp] (JSValue& a, JSValue& b) {
          if (a.is_undefined()) return false;
          if (b.is_undefined()) return true;

          Completion comp_a = js_to_string(vm, a);
          if (comp_a.is_throw()) {
            comp = comp_a;
            throw std::runtime_error("");
          }
          Completion comp_b = js_to_string(vm, b);
          if (comp_b.is_throw()) {
            comp = comp_b;
            throw std::runtime_error("");
          }

          auto prim_a = comp_a.get_value().as_prim_string;
          auto prim_b = comp_b.get_value().as_prim_string;
          return *prim_a < *prim_b;
        });
      }
      catch (std::runtime_error& err) {
        return comp;
      }
    }
    // else, use the compare function
    else if (args.size() >= 1) {
      assert(args[0].is_function());
      try {
        std::sort(data_array.begin(), data_array.end(), [&vm, &args, &comp] (JSValue& a, JSValue& b) {
          if (a.is_undefined()) return false;
          if (b.is_undefined()) return true;

          comp = vm.call_function(args[0], undefined, undefined, {a, b});
          if (comp.is_throw()) {
            throw std::runtime_error("");
          }
          assert(comp.get_value().is_float64());

          return comp.get_value().as_f64 < 0;
        });
      }
      catch (std::runtime_error& err) {
        return comp;
      }
    }
    return This;
  }

  static Completion push(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    JSArray *array = This.as_array;

    return JSFloat(array->push(vm, args));
  }

  static Completion pop(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    return This.as_array->pop();
  }

  static Completion shift(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    return This.as_array->shift();
  }

  static Completion slice(vm_func_This_args_flags) {
    assert(This.is_object() && object_class(This) == CLS_ARRAY);
    auto *arr = This.as_Object<JSArray>();
    int64_t arr_len = arr->get_length();

    int64_t start = TRY_COMP(js_to_int64sat(vm, args.size() > 0 ? args[0] : undefined));
    int64_t end = TRY_COMP(js_to_int64sat(vm, args.size() > 1 ? args[1] : undefined));

    start = std::clamp(start, int64_t(0), arr_len);
    end = std::clamp(end, int64_t(0), arr_len);
    int64_t from = std::min(start, end);
    int64_t to = std::max(start, end);
    int64_t new_len = to - from;

    auto *new_arr = vm.heap.new_object<JSArray>(vm, new_len);
    auto& old_arr = arr->get_dense_array();
    for (int64_t i = 0; i < new_len; i++) {
      new_arr->set_element_fast(vm, i, old_arr[i + from]);
    }
    return JSValue(new_arr);
  }

  static Completion splice(vm_func_This_args_flags) {
    assert(This.is_object() && object_class(This) == CLS_ARRAY);
    auto *arr = This.as_Object<JSArray>();
    auto& dense_arr = arr->get_dense_array();
    int64_t old_len = arr->get_length();

    // nothing to delete or insert
    if (args.size() == 0) {
      return JSValue(vm.heap.new_object<JSArray>(vm, 0));
    }
    int64_t start = TRY_COMP(js_to_int64sat(vm, args[0]));
    int64_t del_cnt = INT64_MAX;
    if (args.size() > 1) {
      del_cnt = TRY_COMP(js_to_int64sat(vm, args[1]));
    }
    del_cnt = del_cnt < 0 ? 0 : del_cnt;

    if (start < 0) {
      if (start < -old_len) {
        start = 0;
      } else {
        start = start + old_len;
      }
    } else if (start >= old_len) {
      del_cnt = 0;
    }
    del_cnt = std::min(del_cnt, old_len - start);

    auto *ret_arr = vm.heap.new_object<JSArray>(vm, del_cnt);
    // remove elements and add them to the return array
    for (u32 i = start; i < start + del_cnt; i++) {
      ret_arr->set_element_fast(vm, i - start, dense_arr[start]);
      dense_arr.erase(dense_arr.begin() + start);
    }

    // insert new elements at `start`
    if (args.size() > 2) {
      u32 insert_cnt = args.size() - 2;
      u32 old_size = dense_arr.size();
      dense_arr.resize(old_size + insert_cnt);

      JSValue *data = dense_arr.data();
      memmove(data + start + insert_cnt, data + start, (old_size - start) * sizeof(*data));

      for (size_t i = start; i < start + (args.size() - 2); i++) {
        arr->set_element_fast(vm, i, args[i - start + 2]);
      }
    }
    arr->resize(dense_arr.size());
    return JSValue(ret_arr);
  }

  static Completion concat(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    auto *array = This.as_array;
    auto *res = vm.heap.new_object<JSArray>(vm, 0);

    // copy `this` array to the result
    auto& res_dense = res->get_dense_array();
    res_dense = array->get_dense_array();

    if (args.size() > 0) [[likely]] {
      for (int i = 0; i < args.size(); i++) {
        JSValue arg = args[i];
        if (arg.is(JSValue::ARRAY)) [[likely]] {
          auto& arg_arr = arg.as_array->get_dense_array();
          res_dense.insert(res_dense.end(), arg_arr.begin(), arg_arr.end());
        } else {
          res_dense.push_back(arg);
        }
      }
    }
    // set the elements to being referenced
    for (JSValue &val : res_dense) {
      set_referenced(val);
    }
    // write barrier
    if (not vm.heap.object_in_newgen(res)) {
      for (JSValue &val : res_dense) {
        vm.heap.write_barrier(res, val);
      }
    }

    res->resize(res_dense.size());
    return JSValue(res);
  }
};

} // njs

#endif //NJS_JSARRAY_PROTOTYPE_H
