#include "NjsVM.h"

namespace njs {

void NjsVM::setup() {
  add_native_func_impl(u"log", NativeFunctions::debug_log, [] (auto& f) {});
  add_native_func_impl(u"$gc", NativeFunctions::js_gc, [] (auto& f) {});
  add_native_func_impl(u"setTimeout", NativeFunctions::set_timeout, [] (auto& f) {});
  add_native_func_impl(u"setInterval", NativeFunctions::set_interval, [] (auto& f) {});
  add_native_func_impl(u"clearTimeout", NativeFunctions::clear_timeout, [] (auto& f) {});
  add_native_func_impl(u"clearInterval", NativeFunctions::clear_interval, [] (auto& f) {});
  add_native_func_impl(u"fetch", NativeFunctions::fetch, [] (auto& f) {});

  add_native_func_impl(u"Object", NativeFunctions::Object_ctor, [this] (JSFunction& func) {
    object_prototype.as_object()->add_prop(StringPool::ATOM_constructor, JSValue(&func));
    func.add_prop(StringPool::ATOM_prototype, object_prototype);
  });

  add_native_func_impl(u"Symbol", NativeFunctions::Symbol, [this] (JSFunction& func) {
    JSValue sym_iterator(heap.new_object<JSSymbol>(u"iterator"));
    func.add_prop(StringPool::ATOM_iterator, sym_iterator);
  });

  add_native_func_impl(u"Error", NativeFunctions::Error_ctor, [] (auto& f) {});

  add_builtin_object(u"console", [this] () {
    JSObject *obj = new_object();

    JSFunctionMeta log_meta {
        .is_anonymous = true,
        .is_native = true,
        .param_count = 0,
        .local_var_count = 0,
        .native_func = NativeFunctions::log,
    };
    JSFunction *log_func = new_function(log_meta);

    obj->add_prop((int64_t) sv_to_atom(u"log"), JSValue(log_func));
    return obj;
  });

  add_builtin_object(u"JSON", [this] () {
    JSObject *obj = new_object();

    JSFunctionMeta meta {
        .is_anonymous = true,
        .is_native = true,
        .param_count = 1,
        .local_var_count = 0,
        .native_func = NativeFunctions::json_stringify,
    };
    JSFunction *func = new_function(meta);

    obj->add_prop((int64_t) sv_to_atom(u"stringify"), JSValue(func));
    return obj;
  });
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
  func->set_prototype(function_prototype);

  builder(*func);
  global_object.as_object()->add_prop(meta.name_index, JSValue(func));
}

void NjsVM::add_builtin_object(const u16string& name,
                               const std::function<JSObject*()>& builder) {
  global_object.as_object()->add_prop(*this, name, JSValue(builder()));
}


}