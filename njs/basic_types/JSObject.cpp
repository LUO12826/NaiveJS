#include "JSObject.h"

#include <array>
#include <sstream>
#include "JSValue.h"
#include "njs/gc/GCHeap.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/testing_and_comparison.h"

namespace njs {

PropFlag PropFlag::empty {};
PropFlag PropFlag::VECW { .enumerable = true, .configurable = true,
                          .writable = true, .has_value = true };
PropFlag PropFlag::VCW {  .configurable = true,
                          .writable = true, .has_value = true };

bool JSObjectProp::operator==(const JSObjectProp& other) const {
  if (flag != other.flag) return false;
  if (flag.is_value()) {
    return same_value(data.value, other.data.value);
  } else if (flag.is_getset()) {
    return same_value(data.getset.getter, other.data.getset.getter)
            && same_value(data.getset.setter, other.data.getset.setter);
  } else {
    return true;
  }
}

Completion JSObject::get_property(NjsVM& vm, JSValue key) {
  if (key.is_atom() && key.val.as_atom == AtomPool::k___proto__) [[unlikely]] {
    return get_proto();
  } else {
    return get_property_impl(vm, key);
  }
}

Completion JSObject::get_property_impl(NjsVM& vm, JSValue key) {
  // fast path for atom
  if (key.is_atom()) [[likely]] {
    return get_prop(vm, key.val.as_atom);
  } else {
    auto comp = js_to_property_key(vm, key);
    if (comp.is_throw()) return comp;
    return get_prop(vm, comp.get_value());
  }
}

ErrorOr<bool> JSObject::set_property(NjsVM& vm, JSValue key, JSValue value, PropFlag flag) {
  if (key.is_atom() && key.val.as_atom == AtomPool::k___proto__) [[unlikely]] {
    return set_proto(value);
  } else {
    return set_property_impl(vm, key, value, flag);
  }
}

ErrorOr<bool> JSObject::set_property_impl(NjsVM& vm, JSValue key, JSValue value, PropFlag flag) {
  auto comp = js_to_property_key(vm, key);
  if (comp.is_throw()) return comp.get_value();
  return set_prop(vm, comp.get_value(), value, flag);
}

Completion JSObject::to_primitive(NjsVM& vm, u16string_view preferred_type) {
  JSValue exotic_to_prim = get_prop(vm, AtomPool::k_toPrimitive).get_value();
  if (exotic_to_prim.is_function()) {
    JSValue hint_arg = vm.new_primitive_string(u16string(preferred_type));
    Completion to_prim_res = vm.call_function(
        exotic_to_prim.val.as_func, JSValue(this), nullptr, {hint_arg});
    if (to_prim_res.is_throw()) {
      return to_prim_res;
    }
    else if (to_prim_res.get_value().is_object()) {
      JSValue err = vm.build_error_internal(JS_TYPE_ERROR, u"");
      return Completion::with_throw(err);
    }
    else {
      return to_prim_res;
    }
  }
  else {
    return ordinary_to_primitive(vm, u"number");
  }

}

Completion JSObject::ordinary_to_primitive(NjsVM& vm, u16string_view hint) {
  std::array<u32, 2> method_names_atom;
  if (hint == u"string") {
    method_names_atom = {AtomPool::k_toString, AtomPool::k_valueOf};
  } else if (hint == u"number") {
    method_names_atom = {AtomPool::k_valueOf, AtomPool::k_toString};
  }

  for (u32 method_atom : method_names_atom) {
    JSValue method = get_prop(vm, method_atom).get_value();
    if (not method.is_function()) continue;

    Completion comp = vm.call_function(method.val.as_func, JSValue(this), nullptr, {});
    if (comp.is_throw()) return comp;

    if (not comp.get_value().is_object()) {
      return comp;
    }
  }

  JSValue err = vm.build_error_internal(JS_TYPE_ERROR, u"");
  return Completion::with_throw(err);
}

ErrorOr<bool> JSObject::set_prop(NjsVM& vm, JSValue key, JSValue value, PropFlag flag) {
  // prop can come from this object or its prototype
  JSObjectProp *desc = get_exist_prop(key);
  if (desc == nullptr) [[unlikely]] {
    if (extensible) {
      desc = &storage[JSObjectKey(key)];
      desc->flag = flag;
      desc->data.value = value;
      return true;
    } else {
      return false;
    }
  } else {
    PropFlag prop_flag = desc->flag;
    if (desc->is_data_descriptor()) {
      if (not prop_flag.writable) return false;
      desc->data.value = value;
      return true;
    } else {
      assert(desc->is_accessor_descriptor());
      JSValue setter = desc->data.getset.setter;
      if (setter.is_undefined()) return false;

      auto comp = vm.call_function(setter.val.as_func, JSValue(this), nullptr, {value});
      return likely(comp.is_normal()) ? ErrorOr<bool>(true) : comp.get_value();
    }
  }
}

bool JSObject::add_prop_trivial(u32 key_atom, JSValue value, PropFlag flag) {
  return add_prop_trivial(JSAtom(key_atom), value, flag);
}

bool JSObject::add_prop_trivial(JSValue key, JSValue value, PropFlag flag) {
  JSObjectProp& prop = storage[JSObjectKey(key)];
  prop.flag = flag;
  prop.data.value = value;
  return true;
}

ErrorOr<bool> JSObject::set_prop(NjsVM& vm, u16string_view key_str, JSValue value, PropFlag flag) {
  return set_prop(vm, JSAtom(vm.str_to_atom(key_str)), value, flag);
}

Completion JSObject::get_prop(NjsVM& vm, u16string_view key_atom) {
  return get_prop(vm, JSAtom(vm.str_to_atom(key_atom)));
}

Completion JSObject::get_prop(NjsVM& vm, JSValue key) {
  JSObjectProp *prop = get_exist_prop(key);
  if (prop == nullptr) [[unlikely]] {
    return prop_not_found;
  } else {
    if (prop->flag.is_value()) [[unlikely]] {
      return prop->data.value;
    } else if (prop->flag.has_getter) {
      JSValue getter = prop->data.getset.getter;
      assert(not getter.is_undefined());
      return vm.call_function(getter.val.as_func, JSValue(this), nullptr, {});
    } else {
      return prop_not_found;
    }
  }
}

bool JSObject::add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl, PropFlag flag) {
  u32 name_idx = vm.str_to_atom(key_str);
  JSFunctionMeta meta {
      .name_index = name_idx,
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = funcImpl,
  };

  return add_prop_trivial(name_idx, JSValue(vm.new_function(meta)));
}

bool JSObject::add_symbol_method(NjsVM& vm, u32 symbol, NativeFuncType funcImpl) {
  JSFunctionMeta meta {
      .is_anonymous = true,
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = funcImpl,
  };

  return add_prop_trivial(JSSymbol(symbol), JSValue(vm.new_function(meta)));
}

void JSObject::gc_scan_children(GCHeap& heap) {
  for (auto& [key, prop]: storage) {
    if (prop.flag.is_value() && prop.data.value.needs_gc()) {
      heap.gc_visit_object(prop.data.value, prop.data.value.as_GCObject());
    }
    if (prop.flag.is_getset()) {
      if (prop.data.getset.getter.needs_gc()) {
        heap.gc_visit_object(prop.data.getset.getter, prop.data.getset.getter.as_GCObject());
      }
      if (prop.data.getset.setter.needs_gc()) {
        heap.gc_visit_object(prop.data.getset.setter, prop.data.getset.setter.as_GCObject());
      }
    }

  }
  if (_proto_.needs_gc()) {
    heap.gc_visit_object(_proto_, _proto_.as_GCObject());
  }
}

std::string JSObject::description() {
  std::ostringstream stream;
  stream << "{ ";

  u32 print_prop_num = std::min((u32)4, (u32)storage.size());
  u32 i = 0;
  for (auto& [key, prop] : storage) {
    if (not prop.flag.enumerable) continue;

    stream << key.to_string() << ": ";
    stream << prop.data.value.description() << ", ";

    i += 1;
    if (i == print_prop_num) break;
  }

  stream << "}";
  return stream.str();
}

std::string JSObject::to_string(NjsVM& vm) const {
  std::string output = "{ ";

  for (auto& [key, prop] : storage) {
    if (not prop.flag.enumerable) continue;

    if (key.type == JSValue::JS_ATOM) {
      u16string escaped = to_escaped_u16string(vm.atom_to_str(key.atom));
      output += to_u8string(escaped);
    }

    output += ": ";
    if (prop.data.value.is(JSValue::STRING)) output += '"';
    output += prop.data.value.to_string(vm);
    if (prop.data.value.is(JSValue::STRING)) output += '"';
    output += ", ";
  }

  output += "}";
  return output;
}

void JSObject::to_json(u16string& output, NjsVM& vm) const {
  output += u"{";

  bool first = true;
  for (auto& [key, prop] : storage) {
    if (not prop.flag.enumerable) continue;
    if (prop.data.value.is_undefined()) continue;

    if (first) first = false;
    else output += u',';
    
    if (key.type == JSValue::JS_ATOM) {
      output += u'"';
      output += to_escaped_u16string(vm.atom_to_str(key.atom));
      output += u'"';
    }

    output += u':';
    prop.data.value.to_json(output, vm);
  }

  output += u"}";
}

}