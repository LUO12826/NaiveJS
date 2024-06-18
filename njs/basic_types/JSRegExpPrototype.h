#ifndef NJS_JS_REGEXP_PROTOTYPE_H
#define NJS_JS_REGEXP_PROTOTYPE_H

#include "JSObject.h"
#include "JSRegExp.h"
#include "JSBoolean.h"
#include "njs/vm/NjsVM.h"
#include "njs/vm/NativeFunction.h"
#include "njs/basic_types/conversion.h"
#include "njs/common/common_def.h"

namespace njs {

class JSRegExpPrototype : public JSObject {
 public:
  explicit JSRegExpPrototype(NjsVM &vm) : JSObject(CLS_REGEXP_PROTO) {
    add_method(vm, u"toString", JSRegExpPrototype::toString);
    add_method(vm, u"test", JSRegExpPrototype::re_test);
    add_method(vm, u"exec", JSRegExpPrototype::re_exec);
    add_symbol_method(vm, AtomPool::k_sym_match, JSRegExpPrototype::re_exec);
    add_symbol_method(vm, AtomPool::k_sym_matchAll, JSRegExpPrototype::re_match_all);
    add_symbol_method(vm, AtomPool::k_sym_replace, JSRegExpPrototype::re_replace);
    add_symbol_method(vm, AtomPool::k_sym_search, JSRegExpPrototype::re_search);
    add_symbol_method(vm, AtomPool::k_sym_split, JSRegExpPrototype::re_split);

    add_prop_trivial(vm, u"global", undefined, PFlag::V);
    add_prop_trivial(vm, u"ignoreCase", undefined, PFlag::V);
    add_prop_trivial(vm, u"multiline", undefined, PFlag::V);
    add_prop_trivial(vm, u"dotAll", undefined, PFlag::V);
    add_prop_trivial(vm, u"unicode", undefined, PFlag::V);
    add_prop_trivial(vm, u"sticky", undefined, PFlag::V);
    add_prop_trivial(vm, u"hasIndices", undefined, PFlag::V);

    set_prop(vm, u"source", JSValue(vm.new_primitive_string(u"(?:)")));
    set_prop(vm, u"flags", JSValue(vm.new_primitive_string(u"")));
  }

  u16string_view get_class_name() override {
    return u"RegExpPrototype";
  }

  static Completion toString(vm_func_This_args_flags) {
    if (This.is_object() && object_class(This) == CLS_REGEXP) [[likely]] {
      return This.as_Object<JSRegExp>()->prototype_to_string(vm);
    } else {
      JSValue err = vm.build_error(JS_TYPE_ERROR,
        u"RegExp.prototype.toString can only be called by RegExp object");
      return CompThrow(err);
    }
  }

  static Completion re_test(vm_func_This_args_flags) {
    assert(This.is_object() && object_class(This) == CLS_REGEXP);

    JSValue arg;
    if (args.size() >= 1) [[likely]] {
      arg = args[0];
    }
    JSValue str = TRYCC(js_to_string(vm, arg));
    return TRY_COMP(This.as_Object<JSRegExp>()->exec(vm, str, true));
  }

  static Completion re_exec(vm_func_This_args_flags) {
    assert(This.is_object() && object_class(This) == CLS_REGEXP);

    JSValue str;
    if (args.size() >= 1) [[likely]] {
      str = args[0];
    }
    return This.as_Object<JSRegExp>()->exec(vm, str, false);
  }

  static Completion re_match_all(vm_func_This_args_flags) {
    return undefined;
  }

  static Completion re_replace(vm_func_This_args_flags) {
    assert(This.is_object() && object_class(This) == CLS_REGEXP);

    JSValue str, replacer;
    if (args.size() >= 1) [[likely]] {
      str = args[0];
    }
    if (args.size() >= 2) [[likely]] {
      replacer = args[1];
    }
    return This.as_Object<JSRegExp>()->replace(vm, str, replacer);
  }

  static Completion re_search(vm_func_This_args_flags) {
    return undefined;
  }

  static Completion re_split(vm_func_This_args_flags) {
    return undefined;
  }

};

}



#endif // NJS_JS_REGEXP_PROTOTYPE_H
