#ifndef NJS_JS_REGEXP_H
#define NJS_JS_REGEXP_H

#include <string>
#include <memory>
#include "njs/common/Defer.h"
#include "JSObject.h"
#include "REByteCode.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/SimpleString.h"
#include "njs/utils/helper.h"
#include "njs/include/libregexp/lre_helper.h"
extern "C" {
#include "njs/include/libregexp/libregexp.h"
}


namespace njs {

struct RegExpFlags {
  bool dotAll;
  bool global;
  bool ignoreCase;
  bool multiline;
  bool sticky;
  bool unicode;
  bool hasIndices;

  explicit RegExpFlags(int flags) {
    global = flags & LRE_FLAG_GLOBAL;
    ignoreCase = flags & LRE_FLAG_IGNORECASE;
    multiline = flags & LRE_FLAG_MULTILINE;
    dotAll = flags & LRE_FLAG_DOTALL;
    unicode = flags & LRE_FLAG_UNICODE;
    sticky = flags & LRE_FLAG_STICKY;
    hasIndices = flags & LRE_FLAG_INDICES;
  }
};

class JSRegExp : public JSObject {
 public:

  static Completion New(NjsVM& vm, u32 pattern_atom, int flags) {
    NOGC;
    auto *regexp = vm.heap.new_object<JSRegExp>(vm, pattern_atom, flags);
    Completion comp = regexp->compile_bytecode_internal((vm));
    return comp.is_throw() ? comp : JSValue(regexp);
  }

  static Completion New(NjsVM& vm, u16string_view pattern, u16string_view flags_str) {
    NOGC;
    auto maybe_flags = str_to_regexp_flags(flags_str);

    if (maybe_flags.has_value()) [[likely]] {
      auto *regexp = vm.heap.new_object<JSRegExp>(vm, pattern, flags_str, maybe_flags.value());
      Completion comp = regexp->compile_bytecode_internal(vm);
      return comp.is_throw() ? comp : JSValue(regexp);
    } else {
      return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid regular expression flags");
    }
  }

  JSRegExp(NjsVM& vm, u16string_view pattern, u16string_view flags_str, int flags)
    : JSObject(vm, CLS_REGEXP, vm.regexp_prototype),
      pattern_atom(vm.str_to_atom(pattern)),
      pattern(pattern),
      flags(flags)
  {
    add_regexp_object_props(vm, flags);

    add_prop_trivial(vm, u"source", JSValue(vm.new_primitive_string(pattern)), PFlag::V);
    add_prop_trivial(vm, u"flags", JSValue(vm.new_primitive_string(flags_str)), PFlag::V);
  }

  JSRegExp(NjsVM& vm, u32 pattern_atom, int flags)
      : JSObject(vm, CLS_REGEXP, vm.regexp_prototype),
        pattern_atom(pattern_atom),
        pattern(vm.atom_to_str(pattern_atom)),
        flags(flags)
  {
    add_regexp_object_props(vm, flags);

    u16string flag_str = regexp_flags_to_str(flags);
    add_prop_trivial(vm, u"source", JSValue(vm.new_primitive_string(pattern.view())), PFlag::V);
    add_prop_trivial(vm, u"flags", JSValue(vm.new_primitive_string(flag_str)), PFlag::V);
  }

  void add_regexp_object_props(NjsVM& vm, int fl) {
    RegExpFlags flags_struct(fl);

    add_prop_trivial(vm, u"global", JSValue(flags_struct.global), PFlag::V);
    add_prop_trivial(vm, u"ignoreCase", JSValue(flags_struct.ignoreCase), PFlag::V);
    add_prop_trivial(vm, u"multiline", JSValue(flags_struct.multiline), PFlag::V);
    add_prop_trivial(vm, u"dotAll", JSValue(flags_struct.dotAll), PFlag::V);
    add_prop_trivial(vm, u"unicode", JSValue(flags_struct.unicode), PFlag::V);
    add_prop_trivial(vm, u"sticky", JSValue(flags_struct.sticky), PFlag::V);
    add_prop_trivial(vm, u"hasIndices", JSValue(flags_struct.hasIndices), PFlag::V);
    add_prop_trivial(vm, u"lastIndex", JSFloat0);
  }

  Completion compile_bytecode_internal(NjsVM& vm) {
    // cached
    if (vm.regexp_bytecode.contains(pattern_atom)) {
      auto& bc = vm.regexp_bytecode[pattern_atom];
      bytecode_len = bc.length;
      bytecode = bc.code.get();
      return undefined;
    }

    char error_msg[64];
    uint8_t *re_bytecode_buf = lre_compile(&this->bytecode_len, error_msg, sizeof(error_msg),
                                           to_u8string(pattern.view()).c_str(), pattern.size(),
                                           flags, nullptr);
    if (re_bytecode_buf == nullptr) {
      char16_t u16_msg[64];
      u8_to_u16_buffer(error_msg, u16_msg);

      return vm.throw_error(JS_SYNTAX_ERROR, u16string(u16_msg));
    }
    // set to cache
    auto& bc = vm.regexp_bytecode[pattern_atom];
    bc.length = bytecode_len;
    bc.code.reset(new uint8_t[bytecode_len]);
    
    memcpy(bc.code.get(), re_bytecode_buf, bytecode_len);
    free(re_bytecode_buf);

    bytecode = bc.code.get();
    return undefined;
  }

  template<typename ItemCB>
  ErrorOr<JSObject *> build_group_object(NjsVM& vm, LREWrapper& lre, ItemCB callback) {
    JSObject *groups = nullptr;
    const char *group_name_ptr = lre.get_groupnames();
    int capture_cnt = lre.get_capture_cnt();
    if (group_name_ptr) {
      groups = vm.new_object(CLS_OBJECT, JSValue::null);
    }

    for (int i = 0; i < capture_cnt; i++) {
      JSValue item;
      if (lre.captured_at_group_index(i)) {
        item = vm.new_primitive_string(lre.get_group_match_result(i));
      }

      if (group_name_ptr && i > 0) {
        if (*group_name_ptr) {
          auto group_name = to_u16string(string(group_name_ptr));
          TRY_ERR(groups->set_prop(vm, group_name, item));
        }
        group_name_ptr += strlen(group_name_ptr) + 1;
      }

      callback(i, item);
    }

    return groups;
  }

  Completion exec(NjsVM& vm, JSValue arg, bool test_mode) {
    arg = TRYCC(js_to_string(vm, arg));
    auto arg_str = arg.as_prim_string->to_std_u16string();

    u32 last_index;
    // last_index is only effective when in `global` or `sticky` mode.
    if (not (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
      last_index = 0;
    } else {
      JSValue last_idx_prop = TRYCC(get_prop(vm, AtomPool::k_lastIndex));
      last_index = TRY_COMP(js_to_uint32(vm, last_idx_prop));

      if (last_index > arg_str.size()) {
        TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat0));
        return JSValue::null;
      }
    }
    
    LREWrapper lre(this->bytecode, arg_str);
    int ret = lre.exec(last_index);

    if (ret == 1) {
      if (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) {
        size_t idx = lre.get_matched_end();
        TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat(idx)));
      }

      if (test_mode) return JSValue(true);

      auto *arr = vm.heap.new_object<JSArray>(vm, lre.get_capture_cnt());

      JSObject *groups = TRY_COMP(build_group_object(vm, lre, [&vm, arr] (int i, JSValue item) {
        arr->set_property_impl(vm, JSFloat(i), item);
      }));

      TRY_COMP(arr->set_prop(vm, u"groups", groups ? JSValue(groups) : undefined));
      size_t index = lre.get_matched_start();
      TRY_COMP(arr->set_prop(vm, u"index", JSFloat(index)));
      TRY_COMP(arr->set_prop(vm, u"input", arg));

      return JSValue(arr);
    }
    else if (ret == 0) {
      // if matching failed, set `lastIndex` to 0
      TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat0));

      if (test_mode) return JSValue(false);
      else return JSValue::null;
    } else {
      // lre out of memory error. assume at this point that it won't happen
      assert(false);
    }
    __builtin_unreachable();
  }

  Completion replace(NjsVM& vm, JSValue str, JSValue replacer) {
    NOGC;
    str = TRYCC(js_to_string(vm, str));
    auto arg_str = str.as_prim_string->to_std_u16string();

    u32 last_index;
    if (not (flags & (LRE_FLAG_STICKY))) {
      last_index = 0;
    } else { // is sticky
      JSValue last_idx_prop = TRYCC(get_prop(vm, AtomPool::k_lastIndex));
      last_index = TRY_COMP(js_to_uint32(vm, last_idx_prop));

      if (last_index > arg_str.size()) {
        TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat0));
        return str;
      }
    }

    u16string replacement;
    bool replacement_is_string = false;
    bool should_populate_replacement = false;

    if (not replacer.is_function()) [[likely]] {
      replacement = TRYCC(js_to_string(vm, replacer)).as_prim_string->to_std_u16string();
      replacement_is_string = true;
      should_populate_replacement = replacement.find(u'$') != u16string::npos;
    }

    LREWrapper lre(this->bytecode, arg_str);
    u16string result;
    u32 prev_last_index = last_index;

    while (last_index < arg_str.size()) {
      int ret = lre.exec(last_index);

      if (ret == 1) {
        u32 first_index = lre.get_matched_start();
        last_index = lre.get_matched_end();

        if (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) {
          TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat(last_index)));
        }

        // the part that is unchanged
        result += arg_str.substr(prev_last_index, first_index - prev_last_index);

        if (replacement_is_string) {
          if (should_populate_replacement) [[unlikely]] {
            u16string_view matched(arg_str.begin() + first_index, arg_str.begin() + last_index);
            u16string populated = prepare_replacer_string(arg_str, replacement, matched,
                                                          first_index, last_index);
            result += populated;
          } else {
            result += replacement;
          }
        } else {
          // match, p1, p2, /* …, */ pN, offset, full string, groups
          vector<JSValue> func_args(lre.get_capture_cnt());

          JSObject *groups = TRY_COMP(build_group_object(vm, lre, [&] (int i, JSValue item) {
            func_args[i] = item;
          }));

          func_args.push_back(JSFloat(first_index));                  // offset
          func_args.push_back(str);                                   // full string
          func_args.push_back(groups ? JSValue(groups) : undefined);  // groups

          JSValue rep_str = TRYCC(vm.call_function(replacer, undefined, undefined, func_args));
          
          rep_str = TRYCC(js_to_string(vm, rep_str));
          result += rep_str.as_prim_string->view();
        }

        prev_last_index = last_index;
        if (not (flags & LRE_FLAG_GLOBAL)) break;
      }
      else if (ret == 0) {
        if (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) {
          TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat0));
        }
        break;
      }
      else {
        assert(false);
      }
    }

    if (prev_last_index < arg_str.size()) {
      result += arg_str.substr(prev_last_index);
    }
    return vm.new_primitive_string(result);
  }

  Completion prototype_to_string(NjsVM& vm) {
    auto flags_str = TRYCC(get_prop(vm, u"flags")).as_prim_string->view();
    u16string regexp_str;
    regexp_str.reserve(pattern.size() + 2 + flags_str.length());
    regexp_str = u'/';
    regexp_str += pattern.view();
    regexp_str += u'/';
    regexp_str += flags_str;
    return JSValue(vm.new_primitive_string(regexp_str));
  }

  u16string_view get_class_name() override {
    return u"RegExp";
  }

  std::string to_string(njs::NjsVM &vm) const override {
    return to_u8string(pattern.view());
  }

  u32 pattern_atom;
  SimpleString pattern;
  int flags;
  int bytecode_len;
  uint8_t *bytecode;
};

} // namespace njs

#endif // NJS_JS_REGEXP_H
