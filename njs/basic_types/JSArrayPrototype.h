#ifndef NJS_JSARRAY_PROTOTYPE_H
#define NJS_JSARRAY_PROTOTYPE_H

#include "JSObject.h"
#include "JSObjectPrototype.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/ArrayRef.h"
#include "njs/common/common_def.h"
#include "JSFunction.h"
#include "njs/vm/Completion.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSArrayIterator.h"
#include "njs/basic_types/conversion.h"

namespace njs {

class NjsVM;

class JSArrayPrototype : public JSObject {
 public:
  JSArrayPrototype(NjsVM& vm) : JSObject(ObjClass::CLS_ARRAY_PROTO) {
    add_symbol_method(vm, AtomPool::k_sym_iterator,  JSArrayPrototype::get_iter);
    add_method(vm, u"at", JSArrayPrototype::at);
    add_method(vm, u"push", JSArrayPrototype::push);
    add_method(vm, u"pop", JSArrayPrototype::pop);
    add_method(vm, u"shift", JSArrayPrototype::shift);
    add_method(vm, u"sort", JSArrayPrototype::sort);
    add_method(vm, u"toString", JSArrayPrototype::toString);
    add_method(vm, u"concat", JSArrayPrototype::concat);
  }

  u16string_view get_class_name() override {
    return u"ArrayPrototype";
  }

  static Completion toString(vm_func_This_args_flags) {
    if (This.is_object() && This.as_object()->get_class() == CLS_ARRAY) [[likely]] {
      auto *arr = This.as_object()->as<JSArray>();
      u16string output;
      bool first = true;

      for (auto& val : arr->dense_array) {
        if (first) first = false;
        else output += u',';

        if (not val.is_nil()) [[likely]] {
          JSValue s = TRY_COMP_COMP(js_to_string(vm, val));
          output += s.val.as_prim_string->str;
        }
      }
      return vm.new_primitive_string(std::move(output));

    } else {
      return JSObjectPrototype::toString(JS_NATIVE_FUNC_ARGS);
    }
  }

  static Completion get_iter(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    assert(This.is_object());
    assert(This.as_object()->get_class() == CLS_ARRAY);

    auto *iter = vm.heap.new_object<JSArrayIterator>(vm, This, JSIteratorKind::VALUE);
    return JSValue(iter);
  }

  static Completion at(vm_func_This_args_flags) {
    assert(args.size() > 0);
    assert(This.is(JSValue::ARRAY));

    JSArray *array = This.val.as_array;
    return array->get_property_impl(vm, args[0]);
  }

  static Completion sort(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    auto& data_array = This.val.as_array->dense_array;

    // no compare function provided. Convert values to strings and do string sorting.
    if (args.size() == 0) {
      std::sort(data_array.begin(), data_array.end(), [&vm] (JSValue& a, JSValue& b) {
        if (a.is_undefined()) return false;
        if (b.is_undefined()) return true;
        return a.to_string(vm) < b.to_string(vm);
      });
    }
    // else, use the compare function
    else if (args.size() >= 1) {
      assert(args[0].is_function());
      Completion comp;
      try {
        std::sort(data_array.begin(), data_array.end(), [&vm, &args, &comp] (JSValue& a, JSValue& b) {
          if (a.is_undefined()) return false;
          if (b.is_undefined()) return true;

          comp = vm.call_function(args[0].val.as_func, undefined, nullptr, {a, b});
          if (comp.is_throw()) {
            throw std::runtime_error("");
          }
          assert(comp.get_value().is_float64());

          return comp.get_value().val.as_f64 < 0;
        });
      }
      catch (std::runtime_error& err) {
        return CompThrow(comp.get_value());
      }

    }

    return undefined;
  }

  static Completion push(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    JSArray *array = This.val.as_array;

    for (size_t i = 0; i < args.size(); i++) {
      array->dense_array.push_back(args[i]);
    }

    JSValue new_length {double(array->dense_array.size())};
    array->set_prop(vm, JSAtom(AtomPool::k_length), new_length);

    return new_length;
  }

  static Completion pop(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    JSArray *array = This.val.as_array;
    auto& dense_arr = array->dense_array;

    if (dense_arr.empty()) [[unlikely]] {
      return undefined;
    } else {
      JSValue last = dense_arr.back();
      if (last.is_uninited()) last.set_undefined();

      dense_arr.pop_back();

      JSValue new_length {double(dense_arr.size())};
      array->set_prop(vm, JSAtom(AtomPool::k_length), new_length);

      return last;
    }
  }

  static Completion shift(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    JSArray *array = This.val.as_array;
    auto& dense_arr = array->dense_array;

    if (dense_arr.empty()) [[unlikely]] {
      return undefined;
    } else {
      JSValue front = dense_arr.front();
      if (front.is_uninited()) front.set_undefined();

      dense_arr.erase(dense_arr.begin());

      JSValue new_length {double(dense_arr.size())};
      array->set_prop(vm, JSAtom(AtomPool::k_length), new_length);

      return front;
    }
  }

  static Completion concat(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    auto *array = This.val.as_array;
    auto *res = vm.heap.new_object<JSArray>(vm, 0);
    // copy the `this` array
    res->dense_array = array->dense_array;

    if (args.size() > 0) [[likely]] {
      for (int i = 0; i < args.size(); i++) {
        JSValue arg = args[i];
        if (arg.is(JSValue::ARRAY)) [[likely]] {
          auto& arg_arr = arg.val.as_array->dense_array;
          res->dense_array.insert(res->dense_array.end(), arg_arr.begin(), arg_arr.end());
          for (auto& ele : arg_arr) {
            res->dense_array.push_back(ele);
          }
        } else {
          res->dense_array.push_back(arg);
        }
      }
    }

    res->update_length();
    return JSValue(res);
  }
};

} // njs

#endif //NJS_JSARRAY_PROTOTYPE_H
