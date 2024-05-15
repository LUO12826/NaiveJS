#include "NjsVM.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/JSErrorPrototype.h"

namespace njs {

inline JSFunctionMeta build_func_meta(NativeFuncType func) {
  return JSFunctionMeta {
      .is_anonymous = true,
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = func,
  };
}

void NjsVM::setup() {
  auto empty_func = [] (auto& f) {};
  add_native_func_impl(u"log", NativeFunction::debug_log, empty_func);
  add_native_func_impl(u"$gc", NativeFunction::js_gc, empty_func);
  add_native_func_impl(u"setTimeout", NativeFunction::set_timeout, empty_func);
  add_native_func_impl(u"setInterval", NativeFunction::set_interval, empty_func);
  add_native_func_impl(u"clearTimeout", NativeFunction::clear_timeout, empty_func);
  add_native_func_impl(u"clearInterval", NativeFunction::clear_interval, empty_func);
  add_native_func_impl(u"fetch", NativeFunction::fetch, empty_func);

  add_error_ctor<JS_ERROR>();
  add_error_ctor<JS_EVAL_ERROR>();
  add_error_ctor<JS_RANGE_ERROR>();
  add_error_ctor<JS_REFERENCE_ERROR>();
  add_error_ctor<JS_SYNTAX_ERROR>();
  add_error_ctor<JS_TYPE_ERROR>();
  add_error_ctor<JS_URI_ERROR>();
  add_error_ctor<JS_INTERNAL_ERROR>();
  add_error_ctor<JS_AGGREGATE_ERROR>();

  add_native_func_impl(u"Object", NativeFunction::Object_ctor, [this] (auto& func) {
    object_prototype.as_object()->add_prop_trivial(AtomPool::k_constructor, JSValue(&func));
    func.add_prop_trivial(AtomPool::k_prototype, object_prototype);
  });

  add_native_func_impl(u"Symbol", NativeFunction::Symbol, [this] (JSFunction& func) {
    JSValue sym_iterator = JSValue::Symbol(atom_pool.atomize_symbol_desc(u"iterator"));
    func.add_prop_trivial(AtomPool::k_iterator, sym_iterator);
  });

  add_builtin_object(u"console", [this] () {
    JSObject *obj = new_object();
    JSFunction *log_func = new_function(build_func_meta(NativeFunction::log));

    obj->add_prop_trivial(str_to_atom(u"log"), JSValue(log_func));
    return obj;
  });

  add_builtin_object(u"JSON", [this] () {
    JSObject *obj = new_object();
    JSFunction *func = new_function(build_func_meta(NativeFunction::json_stringify));

    obj->add_prop_trivial(str_to_atom(u"stringify"), JSValue(func));
    return obj;
  });

  string_const.resize(4);
  string_const[AtomPool::k_undefined] = new_primitive_string(u"undefined");
  string_const[AtomPool::k_null] = new_primitive_string(u"null");
  string_const[AtomPool::k_true] = new_primitive_string(u"true");
  string_const[AtomPool::k_false] = new_primitive_string(u"false");
}

void NjsVM::add_native_func_impl(const u16string& name,
                                 NativeFuncType native_func,
                                 const std::function<void(JSFunction&)>& builder) {
  JSFunctionMeta meta {
      .name_index = str_to_atom(name),
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = native_func,
  };

  auto *func = heap.new_object<JSFunction>(name, meta);
  func->set_proto(function_prototype);
  builder(*func);
  global_object.as_object()->add_prop_trivial(meta.name_index, JSValue(func));
}

void NjsVM::add_builtin_object(const u16string& name,
                               const std::function<JSObject*()>& builder) {
  u32 atom = str_to_atom(name);
  global_object.as_object()->add_prop_trivial(atom, JSValue(builder()));
}


}