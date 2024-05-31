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
extern "C" {
#include "njs/include/libregexp/libregexp.h"
}


namespace njs {

class JSRegExp : public JSObject {
 public:

  static Completion New(NjsVM& vm, u32 pattern_atom, int flags) {
    auto *regexp = vm.heap.new_object<JSRegExp>(vm, pattern_atom, flags);
    Completion comp = regexp->compile_bytecode_internal((vm));
    return comp.is_throw() ? comp : JSValue(regexp);
  }

  static Completion New(NjsVM& vm, const u16string& pattern, const u16string& flags) {
    auto maybe_flags = str_to_regexp_flags(flags);

    if (maybe_flags.has_value()) [[likely]] {
      u32 pattern_atom = vm.str_to_atom(pattern);
      // TODO: optimize this
      auto *regexp = vm.heap.new_object<JSRegExp>(vm, pattern_atom, maybe_flags.value());
      Completion comp = regexp->compile_bytecode_internal((vm));
      return comp.is_throw() ? comp : JSValue(regexp);
    } else {
      JSValue err = vm.build_error_internal(JS_SYNTAX_ERROR, u"Invalid regular expression flags");
      return CompThrow(err);
    }
  }

  JSRegExp(NjsVM& vm, u32 pattern_atom, int flags)
    : JSObject(ObjClass::CLS_REGEXP, vm.regexp_prototype),
      pattern_atom(pattern_atom),
      pattern(vm.atom_to_str(pattern_atom)),
      flags(flags)
  {
    u16string flag_str;
    bool dotAll, global, ignoreCase, multiline, sticky, unicode, hasIndices;
    dotAll = global = ignoreCase = multiline = sticky = unicode = hasIndices = false;

    if (flags & LRE_FLAG_GLOBAL) {
      flag_str += u'g';
      global = true;
    }
    if (flags & LRE_FLAG_IGNORECASE) {
      flag_str += u'i';
      ignoreCase = true;
    }
    if (flags & LRE_FLAG_MULTILINE) {
      flag_str += u'm';
      multiline = true;
    }
    if (flags & LRE_FLAG_DOTALL) {
      flag_str += u's';
      dotAll = true;
    }
    if (flags & LRE_FLAG_UNICODE) {
      flag_str += u'u';
      unicode = true;
    }
    if (flags & LRE_FLAG_STICKY) {
      flag_str += u'y';
      sticky = true;
    }
    if (flags & LRE_FLAG_INDICES) {
      flag_str += u'd';
      hasIndices = true;
    }
    
    add_prop_trivial(vm, u"global", JSValue(global), PFlag::V);
    add_prop_trivial(vm, u"ignoreCase", JSValue(ignoreCase), PFlag::V);
    add_prop_trivial(vm, u"multiline", JSValue(multiline), PFlag::V);
    add_prop_trivial(vm, u"dotAll", JSValue(dotAll), PFlag::V);
    add_prop_trivial(vm, u"unicode", JSValue(unicode), PFlag::V);
    add_prop_trivial(vm, u"sticky", JSValue(sticky), PFlag::V);
    add_prop_trivial(vm, u"hasIndices", JSValue(hasIndices), PFlag::V);

    add_prop_trivial(vm, u"source", JSValue(vm.new_primitive_string(this->pattern)), PFlag::V);
    add_prop_trivial(vm, u"flags", JSValue(vm.new_primitive_string(std::move(flag_str))), PFlag::V);
    add_prop_trivial(vm, u"lastIndex", JSValue(0.0));
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
      int i = 0;
      for (; error_msg[i] != '\0'; i++) {
        u16_msg[i] = error_msg[i];
      }
      u16_msg[i] = '\0';
      JSValue err = vm.build_error_internal(JS_SYNTAX_ERROR, u16string(u16_msg));
      return CompThrow(err);
    }
    // set to cache
    auto& bc = vm.regexp_bytecode[pattern_atom];
    bc.length = bytecode_len;
    bc.code.reset((uint8_t*)malloc(bytecode_len));
    bytecode = bc.code.get();
    memcpy(bc.code.get(), re_bytecode_buf, bytecode_len);
    free(re_bytecode_buf);
    return undefined;
  }

  Completion exec(NjsVM& vm, JSValue val, bool test_mode) {
    assert(val.is_prim_string());
    auto& str = val.val.as_prim_string->str;
    uint8_t *re_bytecode = this->bytecode;

    JSValue last_idx_val = TRY_COMP_COMP(get_prop(vm, AtomPool::k_lastIndex));
    u32 last_index =  TRY_ERR_COMP(js_to_uint32(vm, last_idx_val));

    if (not (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
      last_index = 0;
    }

    uint8_t **capture = nullptr;
    int capture_count = lre_get_capture_count(re_bytecode);
    if (capture_count > 0) {
      capture = new uint8_t*[capture_count * 2];
    }
    Defer defer([capture] { delete[] capture; });

    int ret;
    int shift = 1; // is wide char
    uint8_t *str_buf = (uint8_t*)str.c_str();
    if (last_index > str.size()) {
      ret = 2;
    } else {
      ret = lre_exec(capture, re_bytecode, str_buf, last_index, str.size(), shift, nullptr);
    }

    if (ret != 1) {
      if (ret >= 0) {
        if (ret == 2 || (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
          TRY_ERR_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSValue(0.0)));
        }
      }
      return JSValue::null;
    } else {
      if (test_mode) return JSValue(true);

      if (flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) {
        size_t idx = (capture[1] - str_buf) >> shift;
        TRY_ERR_COMP(set_prop(vm, JSAtom(AtomPool::k_lastIndex), JSValue((double)idx)));
      }

      const char *group_name_ptr = lre_get_groupnames(re_bytecode);

      auto *arr = vm.heap.new_object<JSArray>(vm, capture_count);
      JSObject *groups = nullptr;
      if (group_name_ptr) {
        groups = vm.new_object(CLS_OBJECT, JSValue::null);
      }

      for (int i = 0; i < capture_count; i++) {
        int start, end;
        JSValue val;
        if (capture[2 * i] != NULL && capture[2 * i + 1] != NULL) {
          start = (capture[2 * i] - str_buf) >> shift;
          end = (capture[2 * i + 1] - str_buf) >> shift;
          val = vm.new_primitive_string(str.substr(start, end - start));
        }

        if (group_name_ptr && i > 0) {
          if (*group_name_ptr) {
            auto group_name = to_u16string(to_u8string(group_name_ptr));
            TRY_ERR_COMP(groups->set_prop(vm, group_name, val));
          }
          group_name_ptr += strlen(group_name_ptr) + 1;
        }

        arr->set_element_fast(i, val);
      }

      TRY_ERR_COMP(arr->set_prop(vm, u"groups", groups ? JSValue(groups) : undefined));
      size_t index = (capture[0] - str_buf) >> shift;
      TRY_ERR_COMP(arr->set_prop(vm, u"index", JSValue((double)index)));
      TRY_ERR_COMP(arr->set_prop(vm, u"input", val));

      return JSValue(arr);
    }
  }

  Completion js_to_string(NjsVM& vm) {
    JSValue flags_val = TRY_COMP_COMP(get_prop(vm, u"flags"));
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
