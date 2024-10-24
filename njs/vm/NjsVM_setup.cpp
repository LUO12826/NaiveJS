#include "NjsVM.h"

#include "native.h"
#include "njs/common/common_def.h"
#include "njs/basic_types/JSNumberPrototype.h"
#include "njs/basic_types/JSBooleanPrototype.h"
#include "njs/basic_types/JSStringPrototype.h"
#include "njs/basic_types/JSObjectPrototype.h"
#include "njs/basic_types/JSArrayPrototype.h"
#include "njs/basic_types/JSFunctionPrototype.h"
#include "njs/basic_types/JSErrorPrototype.h"
#include "njs/basic_types/JSRegExpPrototype.h"
#include "njs/basic_types/JSIteratorPrototype.h"
#include "njs/basic_types/JSDatePrototype.h"
#include "njs/basic_types/JSPromisePrototype.h"
#include "njs/basic_types/JSGeneratorPrototype.h"
#include "njs/basic_types/JSPromise.h"

namespace njs {

void NjsVM::init_prototypes() {
  auto *func_proto = heap.new_object<JSFunctionPrototype>(*this);
  function_prototype.set_val(func_proto);
  func_proto->add_methods(*this);

  object_prototype.set_val(heap.new_object<JSObjectPrototype>(*this));
  function_prototype.as_object->set_proto(*this, object_prototype);

  number_prototype.set_val(heap.new_object<JSNumberPrototype>(*this));
  number_prototype.as_object->set_proto(*this, object_prototype);

  boolean_prototype.set_val(heap.new_object<JSBooleanPrototype>(*this));
  boolean_prototype.as_object->set_proto(*this, object_prototype);

  string_prototype.set_val(heap.new_object<JSStringPrototype>(*this));
  string_prototype.as_object->set_proto(*this, object_prototype);

  array_prototype.set_val(heap.new_object<JSArrayPrototype>(*this));
  array_prototype.as_object->set_proto(*this, object_prototype);

  regexp_prototype.set_val(heap.new_object<JSRegExpPrototype>(*this));
  regexp_prototype.as_object->set_proto(*this, object_prototype);

  date_prototype.set_val(heap.new_object<JSDatePrototype>(*this));
  date_prototype.as_object->set_proto(*this, object_prototype);

  promise_prototype.set_val(heap.new_object<JSPromisePrototype>(*this));
  promise_prototype.as_object->set_proto(*this, object_prototype);

  generator_prototype.set_val(heap.new_object<JSGeneratorPrototype>(*this));
  generator_prototype.as_object->set_proto(*this, object_prototype);

  native_error_protos.reserve(JSErrorType::JS_NATIVE_ERROR_COUNT);
  for (int i = 0; i < JSErrorType::JS_NATIVE_ERROR_COUNT; i++) {
    JSObject *proto = heap.new_object<JSErrorPrototype>(*this, (JSErrorType)i);
    proto->set_proto(*this, object_prototype);
    native_error_protos.emplace_back(proto);
  }
  error_prototype = native_error_protos[0];

  iterator_prototype.set_val(heap.new_object<JSIteratorPrototype>(*this));
  iterator_prototype.as_object->set_proto(*this, object_prototype);
}


void NjsVM::setup() {
  add_native_func_impl(u"log", native::misc::debug_log);
  add_native_func_impl(u"___trap", native::misc::debug_trap);
  add_native_func_impl(u"___dummy", native::misc::dummy);
  add_native_func_impl(u"___test", native::misc::_test);
  add_native_func_impl(u"$gc", native::misc::js_gc);
  add_native_func_impl(u"setTimeout", native::misc::setTimeout);
  add_native_func_impl(u"setInterval", native::misc::setInterval);
  add_native_func_impl(u"clearTimeout", native::misc::clearTimeout);
  add_native_func_impl(u"clearInterval", native::misc::clearInterval);
  add_native_func_impl(u"fetch", native::misc::fetch);
  add_native_func_impl(u"isFinite", native::misc::isFinite);
  add_native_func_impl(u"parseFloat", native::misc::parseFloat);
  add_native_func_impl(u"parseInt", native::misc::parseInt);

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
    JSFunction *func = add_native_func_impl(u"Object", native::ctor::Object);
    object_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, object_prototype);
    func->add_method(*this, u"defineProperty", native::Object::defineProperty);
    func->add_method(*this, u"hasOwn", native::Object::hasOwn);
    func->add_method(*this, u"getPrototypeOf", native::Object::getPrototypeOf);
    func->add_method(*this, u"setPrototypeOf", native::Object::setPrototypeOf);
    func->add_method(*this, u"preventExtensions", native::Object::preventExtensions);
    func->add_method(*this, u"isExtensible", native::Object::isExtensible);
    func->add_method(*this, u"create", native::Object::create);
    func->add_method(*this, u"assign", native::Object::assign);
  }

  {
    JSFunction *func = add_native_func_impl(u"Number", native::ctor::Number);
    number_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, number_prototype);
    func->add_method(*this, u"isFinite", native::misc::isFinite);
  }

  {
    JSFunction *func = add_native_func_impl(u"String", native::ctor::String);
    string_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, string_prototype);
    func->add_method(*this, u"fromCharCode", [] (vm_func_This_args_flags) -> Completion {
      assert(args.size() == 1);
      char16_t code = TRY_COMP(js_to_uint16(vm, args[0]));
      return JSValue(vm.new_primitive_string(code));
    });
  }

  {
    JSFunction *func = add_native_func_impl(u"Array", native::ctor::Array);
    array_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, array_prototype);
  }

  {
    JSFunction *func = add_native_func_impl(u"Date", native::ctor::Date);
    date_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, date_prototype);
  }

  {
    JSFunction *func = add_native_func_impl(u"RegExp", native::ctor::RegExp);
    regexp_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, regexp_prototype);
  }

  {
    JSFunction *func = add_native_func_impl(u"Symbol", native::ctor::Symbol);
    func->add_prop_trivial(*this, AtomPool::k_iterator, JSSymbol(AtomPool::k_sym_iterator));
    func->add_prop_trivial(*this, AtomPool::k_match, JSSymbol(AtomPool::k_sym_match));
    func->add_prop_trivial(*this, AtomPool::k_matchAll, JSSymbol(AtomPool::k_sym_matchAll));
    func->add_prop_trivial(*this, AtomPool::k_replace, JSSymbol(AtomPool::k_sym_replace));
    func->add_prop_trivial(*this, AtomPool::k_search, JSSymbol(AtomPool::k_sym_search));
    func->add_prop_trivial(*this, AtomPool::k_split, JSSymbol(AtomPool::k_sym_split));
  }

  {
    JSFunction *func = add_native_func_impl(u"Function", native::ctor::Function);
    function_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, function_prototype);
  }

  {
    JSFunction *func = add_native_func_impl(u"Promise", native::ctor::Promise);
    promise_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, promise_prototype);
  }

  {
    auto *meta = build_func_meta(native::ctor::GeneratorFunction);
    func_meta.emplace_back(meta);
    auto *func = heap.new_object<JSFunction>(*this, u"GeneratorFunction", meta);
    func->set_proto(*this, function_prototype);
    generator_function_ctor.set_val(func);

    generator_prototype.as_object->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, generator_prototype);
  }

  {
    JSObject *obj = add_builtin_object(u"console");
    obj->add_method(*this, u"log", native::misc::log);
  }

  {
    JSObject *obj = add_builtin_object(u"Math");
    obj->add_method(*this, u"min", native::Math::min);
    obj->add_method(*this, u"max", native::Math::max);
    obj->add_method(*this, u"floor", native::Math::floor);
    obj->add_method(*this, u"random", native::Math::random);
  }

  {
    JSObject *obj = add_builtin_object(u"JSON");
    obj->add_method(*this, u"stringify", native::misc::json_stringify);
    obj->add_method(*this, u"parse", native::misc::json_parse);
  }

  add_builtin_global_var(u"undefined", JSValue());
  add_builtin_global_var(u"NaN", JSValue(NAN));
  add_builtin_global_var(u"Infinity", JSValue(1.0 / 0.0));

  JSPromise::add_internal_function_meta(*this);
  JSFunction::add_internal_function_meta(*this);

  string_const.resize(11);
  string_const[AtomPool::k_] = new_primitive_string_ref(atom_to_str(AtomPool::k_));
  string_const[AtomPool::k_undefined] = new_primitive_string_ref(atom_to_str(AtomPool::k_undefined));
  string_const[AtomPool::k_null] = new_primitive_string_ref(atom_to_str(AtomPool::k_null));
  string_const[AtomPool::k_true] = new_primitive_string_ref(atom_to_str(AtomPool::k_true));
  string_const[AtomPool::k_false] = new_primitive_string_ref(atom_to_str(AtomPool::k_false));
  string_const[AtomPool::k_number] = new_primitive_string_ref(atom_to_str(AtomPool::k_number));
  string_const[AtomPool::k_boolean] = new_primitive_string_ref(atom_to_str(AtomPool::k_boolean));
  string_const[AtomPool::k_string] = new_primitive_string_ref(atom_to_str(AtomPool::k_string));
  string_const[AtomPool::k_object] = new_primitive_string_ref(atom_to_str(AtomPool::k_object));
  string_const[AtomPool::k_symbol] = new_primitive_string_ref(atom_to_str(AtomPool::k_symbol));
  string_const[AtomPool::k_function] = new_primitive_string_ref(atom_to_str(AtomPool::k_function));
}

JSFunction* NjsVM::add_native_func_impl(u16string_view name, NativeFuncType native_func) {
  auto *meta = new JSFunctionMeta {
      .name_index = str_to_atom(name),
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = native_func,
  };
  func_meta.emplace_back(meta);

  auto *func = heap.new_object<JSFunction>(*this, name, meta);
  func->set_proto(*this, function_prototype);
  global_object.as_object->add_prop_trivial(*this, meta->name_index, JSValue(func));
  return func;
}

JSObject* NjsVM::add_builtin_object(const u16string& name) {
  u32 atom = str_to_atom(name);
  JSObject *obj = new_object();
  global_object.as_object->add_prop_trivial(*this, atom, JSValue(obj));
  return obj;
}

void NjsVM::add_builtin_global_var(const u16string& name, JSValue val) {
  u32 atom = str_to_atom(name);
  global_object.as_object->add_prop_trivial(*this, atom, val);
}


}