#ifndef NJS_JS_STRING_PROTOTYPE_H
#define NJS_JS_STRING_PROTOTYPE_H

#include <algorithm>
#include "JSObject.h"
#include "JSString.h"
#include "JSFunction.h"
#include "njs/vm/NjsVM.h"
#include "njs/parser/unicode.h"
#include "njs/common/ArrayRef.h"
#include "njs/vm/Completion.h"

namespace njs {

class JSStringPrototype : public JSObject {
 public:
  explicit JSStringPrototype(NjsVM &vm) : JSObject(ObjClass::CLS_STRING_PROTO) {
    add_method(vm, u"charAt", JSStringPrototype::charAt);
    add_method(vm, u"charCodeAt", JSStringPrototype::charCodeAt);
    add_method(vm, u"valueOf", JSStringPrototype::valueOf);
    add_method(vm, u"toLowerCase", JSStringPrototype::toLowerCase);
    add_method(vm, u"toUpperCase", JSStringPrototype::toUpperCase);
    add_method(vm, u"toLocaleLowerCase", JSStringPrototype::toLowerCase);
    add_method(vm, u"toLocaleUpperCase", JSStringPrototype::toUpperCase);
    add_method(vm, u"substring", JSStringPrototype::substring);
    add_method(vm, u"concat", JSStringPrototype::concat);
  }

  u16string_view get_class_name() override {
    return u"StringPrototype";
  }

  static ErrorOr<u16string*> get_string_from_value(NjsVM& vm, JSValue value) {
    if (value.is_prim_string()) {
      return &value.val.as_prim_string->str;
    } else if (value.is(JSValue::STRING_OBJ)) {
      return &value.val.as_string->value.str;
    } else {
      JSValue prim_str = TRY_COMP_ERR(js_to_string(vm, value));
      return &prim_str.val.as_prim_string->str;
    }
  }

  static Completion charAt(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    This = TRY_COMP_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_ERR_COMP(get_string_from_value(vm, This));

    double index = args[0].val.as_f64;
    if (index < 0 || index > str->size()) {
      return vm.new_primitive_string(u"");
    }
    u16string res = u16string{(*str)[(size_t)index]};
    return vm.new_primitive_string(std::move(res));
  }

  static Completion charCodeAt(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    This = TRY_COMP_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_ERR_COMP(get_string_from_value(vm, This));

    double index = args[0].val.as_f64;
    if (index < 0 || index > str->size()) {
      return JSValue(nan(""));
    }
    char16_t ch = (*str)[(size_t)index];
    return JSValue(double(ch));
  }

  static Completion toLowerCase(vm_func_This_args_flags) {
    This = TRY_COMP_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_ERR_COMP(get_string_from_value(vm, This));
    u16string res = *str;
    std::transform(res.begin(), res.end(), res.begin(), character::to_lower_case);
    return vm.new_primitive_string(std::move(res));
  }

  static Completion toUpperCase(vm_func_This_args_flags) {
    This = TRY_COMP_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_ERR_COMP(get_string_from_value(vm, This));
    u16string res = *str;
    std::transform(res.begin(), res.end(), res.begin(), character::to_upper_case);
    return vm.new_primitive_string(std::move(res));
  }

  static Completion substring(vm_func_This_args_flags) {
    This = TRY_COMP_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_ERR_COMP(get_string_from_value(vm, This));

    int64_t start = TRY_ERR_COMP(js_to_int64sat(vm, args.size() > 0 ? args[0] : undefined));
    int64_t end = TRY_ERR_COMP(js_to_int64sat(vm, args.size() > 1 ? args[1] : undefined));
    end = std::max(start, end);

    start = std::clamp(start, int64_t(0), int64_t(str->size() - 1));
    end = std::clamp(end, int64_t(0), int64_t(str->size() - 1));

    return vm.new_primitive_string(str->substr(start, end - start));
  }

  static Completion concat(vm_func_This_args_flags) {
    This = TRY_COMP_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_ERR_COMP(get_string_from_value(vm, This));
    u16string res = *str;

    if (args.size() > 0) [[likely]] {
      for (int i = 0; i < args.size(); i++) {
        u16string *arg_str = TRY_ERR_COMP(get_string_from_value(vm, args[i]));
        res.append(*arg_str);
      }
    }

    return vm.new_primitive_string(std::move(res));
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is(JSValue::STRING)) {
      return This;
    }
    else if (This.is_object() && This.as_object()->get_class() == CLS_STRING) {
      assert(dynamic_cast<JSString*>(This.as_object()));
      auto *str_obj = static_cast<JSString*>(This.as_object());
      return vm.new_primitive_string(str_obj->value.str);
    }
    else {
      JSValue err = vm.build_error_internal(u"String.prototype.valueOf can only accept argument "
                                             "of type string or string object.");
      return CompThrow(err);
    }
  }

};

} // namespace njs

#endif //NJS_JS_STRING_PROTOTYPE_H
