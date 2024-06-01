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
    auto *regexp = vm.heap.new_object<JSRegExp>(vm, pattern_atom, flags);
    Completion comp = regexp->compile_bytecode_internal((vm));
    return comp.is_throw() ? comp : JSValue(regexp);
  }

  static Completion New(NjsVM& vm, const u16string& pattern, const u16string& flags_str) {
    auto maybe_flags = str_to_regexp_flags(flags_str);

    if (maybe_flags.has_value()) [[likely]] {
      auto *regexp = vm.heap.new_object<JSRegExp>(vm, pattern, flags_str, maybe_flags.value());
      Completion comp = regexp->compile_bytecode_internal((vm));
      return comp.is_throw() ? comp : JSValue(regexp);
    } else {
      JSValue err = vm.build_error_internal(JS_SYNTAX_ERROR, u"Invalid regular expression flags");
      return CompThrow(err);
    }
  }

  JSRegExp(NjsVM& vm, const u16string& pattern, const u16string& flags_str, int flags)
    : JSObject(CLS_REGEXP, vm.regexp_prototype),
      pattern_atom(vm.str_to_atom(pattern)),
      pattern(pattern),
      flags(flags)
  {
    add_regexp_object_props(vm, flags);

    add_prop_trivial(vm, u"source", JSValue(vm.new_primitive_string(this->pattern)), PFlag::V);
    add_prop_trivial(vm, u"flags", JSValue(vm.new_primitive_string(flags_str)), PFlag::V);
  }

  JSRegExp(NjsVM& vm, u32 pattern_atom, int flags)
      : JSObject(CLS_REGEXP, vm.regexp_prototype),
        pattern_atom(pattern_atom),
        pattern(vm.atom_to_str(pattern_atom)),
        flags(flags)
  {
    add_regexp_object_props(vm, flags);

    u16string flag_str = regexp_flags_to_str(flags);
    add_prop_trivial(vm, u"source", JSValue(vm.new_primitive_string(this->pattern)), PFlag::V);
    add_prop_trivial(vm, u"flags", JSValue(vm.new_primitive_string(std::move(flag_str))), PFlag::V);
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
    uint8_t *re_bytecode_buf = lre_compile(&this->bytecode_len,
                                           error_msg, sizeof(error_msg),
                                           to_u8string(pattern).c_str(), pattern.size(),
                                           flags, nullptr);
    if (re_bytecode_buf == nullptr) {
      char16_t u16_msg[64];
      u8_to_u16_buffer_convert(error_msg, u16_msg);

      JSValue err = vm.build_error_internal(JS_SYNTAX_ERROR, u16string(u16_msg));
      return CompThrow(err);
    }
    // set to cache
    auto& bc = vm.regexp_bytecode[pattern_atom];
    bc.length = bytecode_len;
    bc.code.reset((uint8_t*)malloc(bytecode_len));
    
    memcpy(bc.code.get(), re_bytecode_buf, bytecode_len);
    free(re_bytecode_buf);

    bytecode = bc.code.get();
    return undefined;
  }

  template<typename ItemCB>
  ErrorOr<JSObject*> build_group_object(NjsVM& vm, LREWrapper& lre, ItemCB callback) {
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
    arg = TRY_COMP(js_to_string(vm, arg));
    auto& arg_str = arg.val.as_prim_string->str;

    u32 last_index;
    // last_index is only effective when in `global` or `sticky` mode.
    if (not (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
      last_index = 0;
    } else {
      JSValue last_idx_prop = TRY_COMP(get_prop(vm, AtomPool::k_lastIndex));
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
        size_t idx = lre.get_last_index();
        TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat(idx)));
      }

      if (test_mode) return JSValue(true);

      auto *arr = vm.heap.new_object<JSArray>(vm, lre.get_capture_cnt());

      JSObject *groups = TRY_COMP(build_group_object(vm, lre, [arr] (int i, JSValue item) {
        arr->set_element_fast(i, item);
      }));

      TRY_COMP(arr->set_prop(vm, u"groups", groups ? JSValue(groups) : undefined));
      size_t index = lre.get_first_index();
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
    str = TRY_COMP(js_to_string(vm, str));
    auto& arg_str = str.val.as_prim_string->str;

    u32 last_index;
    if (not (flags & (LRE_FLAG_STICKY))) {
      last_index = 0;
    } else { // is sticky
      JSValue last_idx_prop = TRY_COMP(get_prop(vm, AtomPool::k_lastIndex));
      last_index = TRY_COMP(js_to_uint32(vm, last_idx_prop));

      if (last_index > arg_str.size()) {
        TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat0));
        return str;
      }
    }

    u16string *replace_str = nullptr;
    JSFunction *replace_func = nullptr;

    if (replacer.is_function()) [[unlikely]] {
      replace_func = replacer.val.as_func;
    } else {
      replace_str = &TRY_COMP(js_to_string(vm, replacer)).val.as_prim_string->str;
    }

    LREWrapper lre(this->bytecode, arg_str);
    u16string result;
    u32 prev_last_index = last_index;

    while (last_index < arg_str.size()) {
      int ret = lre.exec(last_index);

      if (ret == 1) {
        u32 first_index = lre.get_first_index();
        last_index = lre.get_last_index();

        if (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) {
          TRY_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSFloat(last_index)));
        }

        // the part that is unchanged
        result += arg_str.substr(prev_last_index, first_index - prev_last_index);

        if (replace_str) {
          result += *replace_str;
        } else {
          assert(replace_func);
          // match, p1, p2, /* …, */ pN, offset, full string, groups
          vector<JSValue> func_args(lre.get_capture_cnt());

          JSObject *groups = TRY_COMP(build_group_object(vm, lre, [&] (int i, JSValue item) {
            func_args[i] = item;
          }));

          func_args.push_back(JSFloat(first_index));                  // offset
          func_args.push_back(str);                                   // full string
          func_args.push_back(groups ? JSValue(groups) : undefined);  // groups

          JSValue rep_str = TRY_COMP(
            vm.call_function(replace_func, undefined, nullptr, func_args));
          
          rep_str = TRY_COMP(js_to_string(vm, rep_str));
          result += rep_str.val.as_prim_string->str;
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

    return vm.new_primitive_string(std::move(result));
  }

  Completion prototype_to_string(NjsVM& vm) {
    JSValue flags_val = TRY_COMP(get_prop(vm, u"flags"));
    u16string regexp_str = u"/" + pattern + u"/" + flags_val.val.as_prim_string->str;
    return JSValue(vm.new_primitive_string(std::move(regexp_str)));
  }

  u16string_view get_class_name() override {
    return u"RegExp";
  }

  std::string to_string(njs::NjsVM &vm) const override {
    return to_u8string(pattern);
  }

  u32 pattern_atom;
  u16string pattern;
  int flags;
  int bytecode_len;
  uint8_t *bytecode;
};

} // namespace njs

#endif // NJS_JS_REGEXP_H
