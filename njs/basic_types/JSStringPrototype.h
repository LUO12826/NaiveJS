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

using std::vector;
using std::u16string;

class JSStringPrototype : public JSObject {
 public:
  explicit JSStringPrototype(NjsVM &vm) : JSObject(CLS_STRING_PROTO) {
    add_method(vm, u"valueOf", JSStringPrototype::valueOf);
    add_method(vm, u"charAt", JSStringPrototype::charAt);
    add_method(vm, u"charCodeAt", JSStringPrototype::charCodeAt);
    add_method(vm, u"toLowerCase", JSStringPrototype::toLowerCase);
    add_method(vm, u"toUpperCase", JSStringPrototype::toUpperCase);
    add_method(vm, u"toLocaleLowerCase", JSStringPrototype::toLowerCase);
    add_method(vm, u"toLocaleUpperCase", JSStringPrototype::toUpperCase);
    add_method(vm, u"substring", JSStringPrototype::substring);
    add_method(vm, u"substr", JSStringPrototype::substr);
    add_method(vm, u"concat", JSStringPrototype::concat);
    add_method(vm, u"indexOf", JSStringPrototype::indexOf);
    add_method(vm, u"lastIndexOf", JSStringPrototype::lastIndexOf);
    add_method(vm, u"split", JSStringPrototype::split);
    add_method(vm, u"match", JSStringPrototype::match);
    add_method(vm, u"replace", JSStringPrototype::replace);
  }

  u16string_view get_class_name() override {
    return u"StringPrototype";
  }

  static ErrorOr<u16string*> get_string_from_value(NjsVM& vm, JSValue value) {
    if (value.is_prim_string()) {
      return &value.as_prim_string->str;
    } else if (value.is(JSValue::STRING_OBJ)) {
      return &value.as_string->value.str;
    } else {
      JSValue prim_str = TRY_ERR(js_to_string(vm, value));
      return &prim_str.as_prim_string->str;
    }
  }

  static Completion charAt(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));

    double index = args[0].as_f64;
    if (index < 0 || index > str->size()) {
      return vm.new_primitive_string(u"");
    }
    u16string res = u16string{(*str)[(size_t)index]};
    return vm.new_primitive_string(std::move(res));
  }

  static Completion indexOf(vm_func_This_args_flags) {
    assert(args.size() > 0);
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    auto *pattern = TRYCC(js_to_string(vm, args[0])).as_prim_string;

    int64_t start = 0;
    if (args.size() > 1) {
      start = std::max(TRY_COMP(js_to_int64sat(vm, args[1])), int64_t(0));
    }

    if (pattern->length() == 0) [[unlikely]] {
      size_t res = std::min(str->size(), (size_t)start);
      return JSFloat(res);
    }

    size_t find_res = str->find(pattern->str, start);
    if (find_res != u16string::npos) {
      return JSFloat(find_res);
    } else {
      return JSFloat(-1);
    }
  }

  static Completion lastIndexOf(vm_func_This_args_flags) {
    assert(args.size() > 0);
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    auto *pattern = TRYCC(js_to_string(vm, args[0])).as_prim_string;

    int64_t end = INT64_MAX;
    if (args.size() > 1) {
      end = std::min(TRY_COMP(js_to_int64sat(vm, args[1])), INT64_MAX);
    }

    if (pattern->length() == 0) [[unlikely]] {
      size_t res = std::min(str->size(), (size_t)end);
      return JSFloat(res);
    }

    size_t find_res = str->rfind(pattern->str, end);
    if (find_res != u16string::npos) {
      return JSFloat(find_res);
    } else {
      return JSFloat(-1);
    }
  }

  static vector<u16string> cpp_split(const u16string& str, const u16string& delimiter) {
    vector<u16string> tokens;
    if (delimiter.empty()) [[unlikely]] {
      for (auto ch : str) {
        u16string s;
        s += ch;
        tokens.push_back(std::move(s));
      }
      return tokens;
    }
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != u16string::npos) {
      tokens.push_back(str.substr(start, end - start));
      start = end + delimiter.length();
      end = str.find(delimiter, start);
    }

    // Add the last token
    tokens.push_back(str.substr(start));

    return tokens;
  }

  // TODO: should also support regexp.
  static Completion split(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    auto *arr = vm.heap.new_object<JSArray>(vm, 0);

    if (args.size() == 0 || args[0].is_undefined()) [[unlikely]] {
      arr->set_element_fast(0, vm.new_primitive_string(*str));
    }
    else {
      auto *pattern = TRYCC(js_to_string(vm, args[0])).as_prim_string;
      vector<u16string> split_res = cpp_split(*str, pattern->str);

      for (auto& substr : split_res) {
        arr->dense_array.push_back(vm.new_primitive_string(std::move(substr)));
      }
      arr->update_length();
    }
    return JSValue(arr);
  }

  // TODO: pause GC here
  static Completion match(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    JSValue str = TRY_ERR(js_to_string(vm, This));
    
    JSValue match_method;
    // get @@match from that object
    if (args.size() > 0 && args[0].is_object()) [[likely]] {
      JSValue key = JSSymbol(AtomPool::k_sym_match);
      match_method = TRYCC(args[0].as_object->get_property(vm, key));
    }
    
    if (match_method.is_undefined() || not match_method.is_function()) [[unlikely]] {
      JSValue regexp = TRYCC(JSRegExp::New(vm, u"", u""));
      return regexp.as_Object<JSRegExp>()->exec(vm, str, false);
    }
    else {
      return vm.call_function(match_method, args[0], undefined, {str}, flags);
    }
  }

  // TODO: pause GC here
  static Completion replace(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    // nothing to replace
    if (args.size() < 2) {
      return js_to_string(vm, This);
    }
    
    // get @@replace from that object (this is basically for the RegExp)
    if (args[0].is_object()) {
      JSValue key = JSSymbol(AtomPool::k_sym_replace);
      JSValue m_replace = TRYCC(args[0].as_object->get_property(vm, key));
      if (m_replace.is_undefined()) [[unlikely]] {
        goto arg0_is_string;
      } else {
        if (m_replace.is_function()) {
          return vm.call_function(m_replace, args[0], undefined, {This, args[1]}, flags);
        } else {
          return CompThrow(vm.build_error_internal(
            JS_TYPE_ERROR, u"[Symbol.replace] method is not callable"));
        }
      }
    } else {
    arg0_is_string:
      u16string& str = TRYCC(js_to_string(vm, This)).as_prim_string->str;
      JSValue pattern_val = TRYCC(js_to_string(vm, args[0]));
      u16string& pattern = pattern_val.as_prim_string->str;
      u16string res = str;

      auto start_pos = res.find(pattern);

      if (start_pos != u16string::npos) {
        // call a function to get the replacement
        if (args[1].is_function()) {
          vector<JSValue> argv{pattern_val, JSFloat(start_pos)};
          JSValue rep = TRYCC(vm.call_function(args[1], undefined, undefined, argv));
          u16string &replacement = TRYCC(js_to_string(vm, rep)).as_prim_string->str;

          res.replace(start_pos, pattern.size(), replacement);
        }
        else {
          u16string& replacement = TRYCC(js_to_string(vm, args[1])).as_prim_string->str;
          u16string_view matched(str.begin() + start_pos,
                                 str.begin() + start_pos + pattern.size());
          u16string populated = prepare_replacer_string(str, replacement, matched,
                                                        start_pos, start_pos + pattern.size());
          res.replace(start_pos, pattern.size(), populated);
        }
      }

      return vm.new_primitive_string(std::move(res));
    }
  }

  static Completion charCodeAt(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));

    double index = args[0].as_f64;
    if (index < 0 || index > str->size()) {
      return JSValue(nan(""));
    }
    char16_t ch = (*str)[(size_t)index];
    return JSFloat(ch);
  }

  static Completion toLowerCase(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    u16string res = *str;
    std::transform(res.begin(), res.end(), res.begin(), character::to_lower_case);
    return vm.new_primitive_string(std::move(res));
  }

  static Completion toUpperCase(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    u16string res = *str;
    std::transform(res.begin(), res.end(), res.begin(), character::to_upper_case);
    return vm.new_primitive_string(std::move(res));
  }

  static Completion substring(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    int64_t str_len = str->size();

    int64_t start = TRY_COMP(js_to_int64sat(vm, args.size() > 0 ? args[0] : undefined));
    int64_t end = TRY_COMP(js_to_int64sat(vm, args.size() > 1 ? args[1] : undefined));

    start = std::clamp(start, int64_t(0), str_len);
    end = std::clamp(end, int64_t(0), str_len);
    int64_t from = std::min(start, end);
    int64_t to = std::max(start, end);

    return vm.new_primitive_string(str->substr(from, to - from));
  }

  static Completion substr(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    int64_t str_len = str->size();

    int64_t start = TRY_COMP(js_to_int64sat(vm, args.size() > 0 ? args[0] : undefined));
    int64_t length = INT64_MAX;
    if (args.size() > 1) {
      length = TRY_COMP(js_to_int64sat(vm, args[1]));
    }

    start = std::clamp(start, int64_t(0), str_len);
    length = std::clamp(length, int64_t(0), str_len - start);

    return vm.new_primitive_string(str->substr(start, length));
  }

  static Completion concat(vm_func_This_args_flags) {
    TRY_COMP(js_require_object_coercible(vm, This));
    u16string *str = TRY_COMP(get_string_from_value(vm, This));
    u16string res = *str;

    if (args.size() > 0) [[likely]] {
      for (int i = 0; i < args.size(); i++) {
        u16string *arg_str = TRY_COMP(get_string_from_value(vm, args[i]));
        res.append(*arg_str);
      }
    }

    return vm.new_primitive_string(std::move(res));
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is(JSValue::STRING)) {
      return This;
    }
    else if (This.is_object() && object_class(This) == CLS_STRING) {
      assert(dynamic_cast<JSString*>(This.as_object));
      auto *str_obj = static_cast<JSString*>(This.as_object);
      return vm.new_primitive_string(str_obj->value.str);
    }
    else {
      JSValue err = vm.build_error_internal(JS_TYPE_ERROR,
        u"String.prototype.valueOf can only be called by string or string object.");
      return CompThrow(err);
    }
  }

  static Completion String_fromCharCode(vm_func_This_args_flags) {
    assert(args.size() == 1);
    int16_t code = TRY_COMP(js_to_uint16(vm, args[0]));
    return JSValue(vm.new_primitive_string(u16string(1, code)));
  }

};

} // namespace njs

#endif //NJS_JS_STRING_PROTOTYPE_H
