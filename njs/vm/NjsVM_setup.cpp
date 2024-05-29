#include "NjsVM.h"

#include "object_static_method.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/JSErrorPrototype.h"

namespace njs {

void NjsVM::setup() {
  add_native_func_impl(u"log", NativeFunction::debug_log);
  add_native_func_impl(u"$gc", NativeFunction::js_gc);
  add_native_func_impl(u"setTimeout", NativeFunction::set_timeout);
  add_native_func_impl(u"setInterval", NativeFunction::set_interval);
  add_native_func_impl(u"clearTimeout", NativeFunction::clear_timeout);
  add_native_func_impl(u"clearInterval", NativeFunction::clear_interval);
  add_native_func_impl(u"fetch", NativeFunction::fetch);

  add_error_ctor<JS_ERROR>();
  add_error_ctor<JS_EVAL_ERROR>();
  add_error_ctor<JS_RANGE_ERROR>();
  add_error_ctor<JS_REFERENCE_ERROR>();
  add_error_ctor<JS_SYNTAX_ERROR>();
  add_error_ctor<JS_TYPE_ERROR>();
  add_error_ctor<JS_URI_ERROR>();
  add_error_ctor<JS_INTERNAL_ERROR>();
  add_error_ctor<JS_AGGREGATE_ERROR>();

  {
    JSFunction *func = add_native_func_impl(u"Object", NativeFunction::Object_ctor);
    object_prototype.as_object()->add_prop_trivial(AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(AtomPool::k_prototype, object_prototype);
    func->add_method(*this, u"defineProperty", Object_defineProperty);
    func->add_method(*this, u"hasOwn", Object_hasOwn);
    func->add_method(*this, u"getPrototypeOf", Object_getPrototypeOf);
    func->add_method(*this, u"preventExtensions", Object_preventExtensions);
    func->add_method(*this, u"isExtensible", Object_isExtensible);
    func->add_method(*this, u"create", Object_create);
  }

  {
    JSFunction *func = add_native_func_impl(u"Number", NativeFunction::Number_ctor);
    number_prototype.as_object()->add_prop_trivial(AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(AtomPool::k_prototype, number_prototype);
  }

  {
    JSFunction *func = add_native_func_impl(u"String", NativeFunction::String_ctor);
    string_prototype.as_object()->add_prop_trivial(AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(AtomPool::k_prototype, string_prototype);
  }

  {
    JSFunction *func = add_native_func_impl(u"Symbol", NativeFunction::Symbol);
    func->add_prop_trivial(AtomPool::k_iterator, JSSymbol(AtomPool::k_sym_iterator));
    func->add_prop_trivial(AtomPool::k_match, JSSymbol(AtomPool::k_sym_match));
    func->add_prop_trivial(AtomPool::k_matchAll, JSSymbol(AtomPool::k_sym_matchAll));
    func->add_prop_trivial(AtomPool::k_replace, JSSymbol(AtomPool::k_sym_replace));
    func->add_prop_trivial(AtomPool::k_search, JSSymbol(AtomPool::k_sym_search));
    func->add_prop_trivial(AtomPool::k_split, JSSymbol(AtomPool::k_sym_split));
  }

  {
    JSObject *obj = add_builtin_object(u"console");
    obj->add_method(*this, u"log", NativeFunction::log);
  }

  {
    JSObject *obj = add_builtin_object(u"JSON");
    obj->add_method(*this, u"stringify", NativeFunction::json_stringify);
  }

  add_builtin_global_var(u"undefined", JSValue());
  add_builtin_global_var(u"NaN", JSValue(nan("")));
  add_builtin_global_var(u"Infinity", JSValue(1.0 / 0.0));

  string_const.resize(20);
  string_const[AtomPool::k_] = new_primitive_string(u"");
  string_const[AtomPool::k_undefined] = new_primitive_string(u"undefined");
  string_const[AtomPool::k_null] = new_primitive_string(u"null");
  string_const[AtomPool::k_true] = new_primitive_string(u"true");
  string_const[AtomPool::k_false] = new_primitive_string(u"false");
  string_const[AtomPool::k_number] = new_primitive_string(u"number");
  string_const[AtomPool::k_boolean] = new_primitive_string(u"boolean");
  string_const[AtomPool::k_string] = new_primitive_string(u"string");
  string_const[AtomPool::k_object] = new_primitive_string(u"object");
  string_const[AtomPool::k_symbol] = new_primitive_string(u"symbol");
  string_const[AtomPool::k_function] = new_primitive_string(u"function");
}

JSFunction* NjsVM::add_native_func_impl(const u16string& name, NativeFuncType native_func) {
  JSFunctionMeta meta {
      .name_index = str_to_atom(name),
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = native_func,
  };

  auto *func = heap.new_object<JSFunction>(*this, name, meta);
  func->set_proto(function_prototype);
  global_object.as_object()->add_prop_trivial(meta.name_index, JSValue(func));
  return func;
}

JSObject* NjsVM::add_builtin_object(const u16string& name) {
  u32 atom = str_to_atom(name);
  JSObject *obj = new_object();
  global_object.as_object()->add_prop_trivial(atom, JSValue(obj));
  return obj;
}

void NjsVM::add_builtin_global_var(const u16string& name, JSValue val) {
  u32 atom = str_to_atom(name);
  global_object.as_object()->add_prop_trivial(atom, val);
}


}