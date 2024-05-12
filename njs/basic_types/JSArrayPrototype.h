#ifndef NJS_JSARRAY_PROTOTYPE_H
#define NJS_JSARRAY_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/ArrayRef.h"
#include "njs/common/common_def.h"
#include "JSFunction.h"
#include "njs/vm/Completion.h"
#include "njs/basic_types/JSArray.h"

namespace njs {

class NjsVM;

class JSArrayPrototype : public JSObject {
 public:
  JSArrayPrototype(NjsVM& vm) : JSObject(ObjClass::CLS_ARRAY_PROTO) {
    add_method(vm, u"at", JSArrayPrototype::at);
    add_method(vm, u"push", JSArrayPrototype::push);
    add_method(vm, u"sort", JSArrayPrototype::sort);
  }

  u16string_view get_class_name() override {
    return u"ArrayPrototype";
  }

  static Completion at(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    assert(This.is(JSValue::ARRAY));

    JSArray *array = This.val.as_array;
    double index = args[0].val.as_f64;
    if (index < 0 || index > array->get_length()) {
      return undefined;
    } else {
      return array->access_element((u32)index, false);
    }
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
        return Completion::with_throw(comp.get_value());
      }

    }

    return undefined;
  }

  static Completion push(vm_func_This_args_flags) {
    assert(This.is(JSValue::ARRAY));
    JSArray *array = This.val.as_array;

    size_t old_size = array->dense_array.size();
    size_t new_size = old_size + args.size();

    array->dense_array.resize(new_size);

    for (size_t i = 0; i < args.size(); i++) {
      array->dense_array[old_size + i].assign(args[i]);
    }

    array->set_length(u32(new_size));

    // return the length after push
    return JSValue(double(new_size));
  }
};

} // njs

#endif //NJS_JSARRAY_PROTOTYPE_H
