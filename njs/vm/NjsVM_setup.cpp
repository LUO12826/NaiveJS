#include "NjsVM.h"

namespace njs {

void NjsVM::setup() {
  add_native_func_impl(u"log", InternalFunctions::log);
  add_native_func_impl(u"$gc", InternalFunctions::js_gc);
  add_native_func_impl(u"setTimeout", InternalFunctions::set_timeout);
  add_native_func_impl(u"setInterval", InternalFunctions::set_interval);
  add_native_func_impl(u"clearTimeout", InternalFunctions::clear_timeout);
  add_native_func_impl(u"clearInterval", InternalFunctions::clear_interval);
  add_native_func_impl(u"fetch", InternalFunctions::fetch);

  add_builtin_object(u"console", [] (GCHeap& heap, StringPool& str_pool) {
    JSObject *obj = heap.new_object<JSObject>();

    JSFunctionMeta log_meta {
        .is_anonymous = true,
        .is_native = true,
        .param_count = 0,
        .local_var_count = 0,
        .native_func = InternalFunctions::log,
    };
    JSFunction *log_func = heap.new_object<JSFunction>(log_meta);

    JSValue key(JSValue::JS_ATOM);
    key.val.as_int64 = str_pool.add_string(u"log");

    obj->add_prop(key, JSValue(log_func));
    return obj;
  });

  add_builtin_object(u"JSON", [] (GCHeap& heap, StringPool& str_pool) {
    JSObject *obj = heap.new_object<JSObject>();

    JSFunctionMeta func_meta {
        .is_anonymous = true,
        .is_native = true,
        .param_count = 1,
        .local_var_count = 0,
        .native_func = InternalFunctions::json_stringify,
    };
    JSFunction *func = heap.new_object<JSFunction>(func_meta);

    auto key_atom_value = str_pool.add_string(u"stringify");
    obj->add_prop(JSValue::Atom(key_atom_value), JSValue(func));

    return obj;
  });
}


}