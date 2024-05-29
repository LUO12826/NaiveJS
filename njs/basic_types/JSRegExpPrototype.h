#ifndef NJS_JS_REGEXP_PROTOTYPE_H
#define NJS_JS_REGEXP_PROTOTYPE_H

#include "JSObject.h"
#include "JSBoolean.h"
#include "njs/vm/NjsVM.h"
#include "njs/vm/NativeFunction.h"
#include "njs/common/common_def.h"

namespace njs {

class JSRegExpPrototype : public JSObject {
 public:
  explicit JSRegExpPrototype(NjsVM &vm) : JSObject(ObjClass::CLS_REGEXP_PROTO) {
    add_method(vm, u"toString", JSRegExpPrototype::toString);
    add_method(vm, u"test", JSRegExpPrototype::re_test);
    add_method(vm, u"exec", JSRegExpPrototype::re_exec);
    add_symbol_method(vm, AtomPool::k_sym_match, JSRegExpPrototype::re_match);
    add_symbol_method(vm, AtomPool::k_sym_matchAll, JSRegExpPrototype::re_match_all);
    add_symbol_method(vm, AtomPool::k_sym_replace, JSRegExpPrototype::re_replace);
    add_symbol_method(vm, AtomPool::k_sym_search, JSRegExpPrototype::re_search);
    add_symbol_method(vm, AtomPool::k_sym_split, JSRegExpPrototype::re_split);

    set_prop(vm, u"global", undefined, PropFlag::V);
    set_prop(vm, u"ignoreCase", undefined, PropFlag::V);
    set_prop(vm, u"multiline", undefined, PropFlag::V);
    set_prop(vm, u"dotAll", undefined, PropFlag::V);
    set_prop(vm, u"unicode", undefined, PropFlag::V);
    set_prop(vm, u"sticky", undefined, PropFlag::V);
    set_prop(vm, u"hasIndices", undefined, PropFlag::V);

    set_prop(vm, u"source", JSValue(vm.new_primitive_string(u"(?:)")));
    set_prop(vm, u"flags", JSValue(vm.new_primitive_string(u"")));
  }

  u16string_view get_class_name() override {
    return u"RegExpPrototype";
  }

  static Completion toString(vm_func_This_args_flags) {
    return undefined;
  }

  static Completion re_test(vm_func_This_args_flags) {
    assert(This.is_object() && This.as_object()->get_class() == CLS_REGEXP);

    JSValue arg;
    if (args.size() >= 1) [[likely]] {
      arg = args[0];
    }
    JSValue str = TRY_COMP_COMP(js_to_string(vm, arg));
    JSValue res = TRY_COMP_COMP(This.as_object()->as<JSRegExp>()->exec(vm, str, true));

    return res.is(JSValue::BOOLEAN) ? res : JSValue(false);
  }

  static Completion re_exec(vm_func_This_args_flags) {
    assert(This.is_object() && This.as_object()->get_class() == CLS_REGEXP);

    JSValue arg;
    if (args.size() >= 1) [[likely]] {
      arg = args[0];
    }
    JSValue str = TRY_COMP_COMP(js_to_string(vm, arg));

    return This.as_object()->as<JSRegExp>()->exec(vm, str, false);
  }

  static Completion re_match(vm_func_This_args_flags) {
    return undefined;
  }

  static Completion re_match_all(vm_func_This_args_flags) {
    return undefined;
  }

  static Completion re_replace(vm_func_This_args_flags) {
    return undefined;
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
