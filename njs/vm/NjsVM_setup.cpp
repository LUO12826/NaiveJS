#include "NjsVM.h"

#include "njs/basic_types/GlobalObject.h"

namespace njs {

void NjsVM::setup() {
  add_native_func_impl(u"log", InternalFunctions::debug_log, [] (auto& f) {});
  add_native_func_impl(u"$gc", InternalFunctions::js_gc, [] (auto& f) {});
  add_native_func_impl(u"setTimeout", InternalFunctions::set_timeout, [] (auto& f) {});
  add_native_func_impl(u"setInterval", InternalFunctions::set_interval, [] (auto& f) {});
  add_native_func_impl(u"clearTimeout", InternalFunctions::clear_timeout, [] (auto& f) {});
  add_native_func_impl(u"clearInterval", InternalFunctions::clear_interval, [] (auto& f) {});
  add_native_func_impl(u"fetch", InternalFunctions::fetch, [] (auto& f) {});

  add_native_func_impl(u"Object", InternalFunctions::Object_ctor, [this] (JSFunction& func) {
    object_prototype.as_object()->add_prop(StringPool::ATOM_constructor, JSValue(&func));
    func.add_prop(StringPool::ATOM_prototype, object_prototype);
  });

  add_native_func_impl(u"Error", InternalFunctions::Error_ctor, [] (auto& f) {});

  add_builtin_object(u"console", [this] () {
    JSObject *obj = new_object();

    JSFunctionMeta log_meta {
        .is_anonymous = true,
        .is_native = true,
        .param_count = 0,
        .local_var_count = 0,
        .native_func = InternalFunctions::log,
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
        .native_func = InternalFunctions::json_stringify,
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