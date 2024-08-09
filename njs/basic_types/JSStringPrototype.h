#ifndef NJS_JS_STRING_PROTOTYPE_H
#define NJS_JS_STRING_PROTOTYPE_H

#include <algorithm>
#include "JSObject.h"
#include "JSString.h"
#include "JSArray.h"
#include "JSRegExp.h"
#include "JSFunction.h"
#include "njs/vm/NjsVM.h"
#include "njs/parser/unicode.h"
#include "njs/common/Span.h"
#include "njs/common/Completion.h"

#define REQUIRE_COERCIBLE(x)                                                                      \
  if ((x).is_nil()) [[unlikely]] {                                                                \
    return vm.throw_error(JS_TYPE_ERROR, u"undefined or null is not coercible");                  \
  }

namespace njs {

using std::vector;
using std::u16string;

class JSStringPrototype : public JSObject {
 public:
  explicit JSStringPrototype(NjsVM &vm) : JSObject(CLS_STRING_PROTO) {
    add_method(vm, u"valueOf", JSStringPrototype::valueOf);
    add_method(vm, u"toString", JSStringPrototype::valueOf);
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

  static ErrorOr<u16string_view> get_string_from_value(NjsVM& vm, JSValue value) {
    if (value.is_prim_string()) {
      return value.as_prim_string->view();
    } else if (value.is(JSValue::STRING_OBJ)) {
      return value.as_string->get_prim_value()->view();
    } else {
      JSValue prim_str = TRY_ERR(js_to_string(vm, value));
      return prim_str.as_prim_string->view();
    }
  }

  static ErrorOr<PrimitiveString *> get_prim_string_from_value(NjsVM& vm, JSValue value) {
    if (value.is_prim_string()) {
      return value.as_prim_string;
    } else if (value.is(JSValue::STRING_OBJ)) {
      return value.as_string->get_prim_value();
    } else {
      return TRY_ERR(js_to_string(vm, value)).as_prim_string;
    }
  }

  static Completion charAt(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    REQUIRE_COERCIBLE(This);
    PrimitiveString *str = TRY_COMP(get_prim_string_from_value(vm, This));

    double index = args[0].as_f64;
    if (index < 0 || index > str->length()) [[unlikely]] {
      return vm.new_primitive_string(u"");
    }
    return vm.new_primitive_string((*str)[(size_t)index]);
  }

  static Completion indexOf(vm_func_This_args_flags) {
    assert(args.size() > 0);
    REQUIRE_COERCIBLE(This);
    u16string_view str = TRY_COMP(get_string_from_value(vm, This));
    u16string_view pattern = TRYCC(js_to_string(vm, args[0])).as_prim_string->view();

    int64_t start = 0;
    if (args.size() > 1) {
      start = std::max(TRY_COMP(js_to_int64sat(vm, args[1])), int64_t(0));
    }

    if (pattern.length() == 0) [[unlikely]] {
      size_t res = std::min(str.size(), (size_t)start);
      return JSFloat(res);
    }

    size_t find_res = str.find(pattern, start);
    if (find_res != u16string::npos) {
      return JSFloat(find_res);
    } else {
      return JSFloat(-1);
    }
  }

  static Completion lastIndexOf(vm_func_This_args_flags) {
    assert(args.size() > 0);
    REQUIRE_COERCIBLE(This);
    u16string_view str = TRY_COMP(get_string_from_value(vm, This));
    u16string_view pattern = TRYCC(js_to_string(vm, args[0])).as_prim_string->view();

    int64_t end = INT64_MAX;
    if (args.size() > 1) {
      end = std::min(TRY_COMP(js_to_int64sat(vm, args[1])), INT64_MAX);
    }

    if (pattern.length() == 0) [[unlikely]] {
      size_t res = std::min(str.size(), (size_t)end);
      return JSFloat(res);
    }

    size_t find_res = str.rfind(pattern, end);
    if (find_res != u16string::npos) {
      return JSFloat(find_res);
    } else {
      return JSFloat(-1);
    }
  }

  static vector<u16string_view> cpp_split(u16string_view str, u16string_view delimiter) {
    vector<u16string_view> tokens;
    if (delimiter.empty()) [[unlikely]] {
      for (size_t i = 0; i < str.size(); i++) {
        tokens.emplace_back(str.data() + i, 1);
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
    tokens.emplace_back(str.substr(start));

    return tokens;
  }

  // TODO: should also support regexp.
  static Completion split(vm_func_This_args_flags) {
    REQUIRE_COERCIBLE(This);
    u16string_view str = TRY_COMP(get_string_from_value(vm, This));
    auto *arr = vm.heap.new_object<JSArray>(vm, 0);

    if (args.empty() || args[0].is_undefined()) [[unlikely]] {
      arr->push(vm, vm.new_primitive_string(str));
    }
    else {
      auto *pattern = TRYCC(js_to_string(vm, args[0])).as_prim_string;
      vector<u16string_view> split_res = cpp_split(str, pattern->view());

      for (auto& substr : split_res) {
        arr->push(vm, vm.new_primitive_string(substr));
      }
    }
    return JSValue(arr);
  }


  static Completion match(vm_func_This_args_flags) {
    NOGC;
    REQUIRE_COERCIBLE(This);
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
      return vm.call_function(match_method, args[0], undefined, {&str, 1}, flags);
    }
  }

  static Completion replace(vm_func_This_args_flags) {
    NOGC;
    REQUIRE_COERCIBLE(This);
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
          JSValue argv[] {This, args[1]};
          return vm.call_function(m_replace, args[0], undefined, {argv, 2} , flags);
        } else {
          return vm.throw_error(JS_TYPE_ERROR, u"[Symbol.replace] method is not callable");
        }
      }
    } else {
    arg0_is_string:
      u16string_view str = TRYCC(js_to_string(vm, This)).as_prim_string->view();
      JSValue pattern_val = TRYCC(js_to_string(vm, args[0]));
      u16string_view pattern = pattern_val.as_prim_string->view();
      u16string res(str);

      auto start_pos = res.find(pattern);

      if (start_pos != u16string::npos) {
        // call a function to get the replacement
        if (args[1].is_function()) {
          JSValue argv[2] {pattern_val, JSFloat(start_pos)};
          JSValue rep = TRYCC(vm.call_function(args[1], undefined, undefined, {argv, 2}));
          u16string_view replacement = TRYCC(js_to_string(vm, rep)).as_prim_string->view();

          res.replace(start_pos, pattern.size(), replacement);
        }
        else {
          u16string_view replacement = TRYCC(js_to_string(vm, args[1])).as_prim_string->view();
          u16string_view matched(str.begin() + start_pos,
                                 str.begin() + start_pos + pattern.size());
          u16string populated = prepare_replacer_string(str, replacement, matched,
                                                        start_pos, start_pos + pattern.size());
          res.replace(start_pos, pattern.size(), populated);
        }
      }

      return vm.new_primitive_string(res);
    }
  }

  static Completion charCodeAt(vm_func_This_args_flags) {
    assert(args.size() > 0 && args[0].is(JSValue::NUM_FLOAT));
    REQUIRE_COERCIBLE(This);
    PrimitiveString *str = TRY_COMP(get_prim_string_from_value(vm, This));

    double index = args[0].as_f64;
    if (index < 0 || index > str->length()) {
      return JSValue(NAN);
    }
    char16_t ch = (*str)[(size_t)index];
    return JSFloat(ch);
  }

  static Completion toLowerCase(vm_func_This_args_flags) {
    REQUIRE_COERCIBLE(This);
    u16string_view str = TRY_COMP(get_string_from_value(vm, This));
    u16string res(str);
    std::transform(res.begin(), res.end(), res.begin(), character::to_lower_case);
    return vm.new_primitive_string(res);
  }

  static Completion toUpperCase(vm_func_This_args_flags) {
    REQUIRE_COERCIBLE(This);
    u16string_view str = TRY_COMP(get_string_from_value(vm, This));
    u16string res(str);
    std::transform(res.begin(), res.end(), res.begin(), character::to_upper_case);
    return vm.new_primitive_string(res);
  }

  static Completion substring(vm_func_This_args_flags) {
    REQUIRE_COERCIBLE(This);
    PrimitiveString *str = TRY_COMP(get_prim_string_from_value(vm, This));
    int64_t str_len = str->length();

    int64_t start = TRY_COMP(js_to_int64sat(vm, args.size() > 0 ? args[0] : undefined));
    int64_t end = INT64_MAX;
    if (args.size() > 1) {
      end = TRY_COMP(js_to_int64sat(vm, args[1]));
    }

    start = std::clamp(start, int64_t(0), str_len);
    end = std::clamp(end, int64_t(0), str_len);
    int64_t from = std::min(start, end);
    int64_t to = std::max(start, end);

    return JSValue(str->substr(vm.heap, from, to - from));
  }

  static Completion substr(vm_func_This_args_flags) {
    REQUIRE_COERCIBLE(This);
    PrimitiveString *str = TRY_COMP(get_prim_string_from_value(vm, This));
    int64_t str_len = str->length();

    int64_t start = TRY_COMP(js_to_int64sat(vm, args.size() > 0 ? args[0] : undefined));
    int64_t length = INT64_MAX;
    if (args.size() > 1) {
      length = TRY_COMP(js_to_int64sat(vm, args[1]));
    }

    start = std::clamp(start, int64_t(0), str_len);
    length = std::clamp(length, int64_t(0), str_len - start);

    return JSValue(str->substr(vm.heap, start, length));
  }

  static Completion concat(vm_func_This_args_flags) {
    REQUIRE_COERCIBLE(This);
    PrimitiveString *str = TRY_COMP(get_prim_string_from_value(vm, This));

    if (args.size() == 1) [[likely]] {
      u16string_view arg_str = TRY_COMP(get_string_from_value(vm, args[0]));
      auto res = str->concat(vm.heap, arg_str.data(), arg_str.size());
      return JSValue(res);
    }
    else if (args.size() > 1) {
      u16string res(str->view());
      for (int i = 0; i < args.size(); i++) {
        u16string_view arg_str = TRY_COMP(get_string_from_value(vm, args[i]));
        res.append(arg_str);
      }
      return vm.new_primitive_string(res); 
    }
    else {
      return vm.new_primitive_string(str->view());
    }
  }

  static Completion valueOf(vm_func_This_args_flags) {
    if (This.is(JSValue::STRING)) {
      return This;
    }
    else if (This.is_object() && object_class(This) == CLS_STRING) {
      assert(dynamic_cast<JSString*>(This.as_object));
      auto *str_obj = static_cast<JSString*>(This.as_object);
      return JSValue(str_obj->get_prim_value());
    }
    else {
      JSValue err = vm.build_error(JS_TYPE_ERROR,
        u"String.prototype.valueOf can only be called by string or string object.");
      return CompThrow(err);
    }
  }

  static Completion String_fromCharCode(vm_func_This_args_flags) {
    assert(args.size() == 1);
    char16_t code = TRY_COMP(js_to_uint16(vm, args[0]));
    return JSValue(vm.new_primitive_string(code));
  }

};

} // namespace njs

#endif //NJS_JS_STRING_PROTOTYPE_H
