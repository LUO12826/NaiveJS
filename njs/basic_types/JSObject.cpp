#include "JSObject.h"

#include <array>
#include <sstream>
#include "JSValue.h"
#include "njs/gc/GCHeap.h"
#include "njs/vm/NjsVM.h"
#include "njs/basic_types/testing_and_comparison.h"

namespace njs {

PFlag PFlag::empty {};
PFlag PFlag::VECW { .enumerable = true, .configurable = true,
                          .writable = true, .has_value = true };
PFlag PFlag::VCW {  .configurable = true,
                          .writable = true, .has_value = true };
PFlag PFlag::V { .has_value = true };

bool JSPropDesc::operator==(const JSPropDesc& other) const {
  if (flag != other.flag) return false;
  if (flag.is_value()) {
    return same_value(data.value, other.data.value);
  } else if (flag.is_getset()) {
    return (!flag.has_getter || same_value(data.getset.getter, other.data.getset.getter)) &&
           (!flag.has_setter || same_value(data.getset.setter, other.data.getset.setter));
  } else {
    return true;
  }
}

Completion JSObject::get_property(NjsVM& vm, JSValue key) {
  if (key.is_atom() && key.u.as_atom == AtomPool::k___proto__) [[unlikely]] {
    return get_proto();
  } else {
    return get_property_impl(vm, key);
  }
}

Completion JSObject::get_property_impl(NjsVM& vm, JSValue key) {
  // fast path for atom
  if (key.is_atom()) [[likely]] {
    return get_prop(vm, key.u.as_atom);
  } else {
    auto comp = js_to_property_key(vm, key);
    if (comp.is_throw()) return comp;
    return get_prop(vm, comp.get_value());
  }
}

ErrorOr<bool> JSObject::set_property(NjsVM& vm, JSValue key, JSValue value) {
  if (key.is_atom() && key.u.as_atom == AtomPool::k___proto__) [[unlikely]] {
    return set_proto(value);
  } else {
    return set_property_impl(vm, key, value);
  }
}

ErrorOr<bool> JSObject::delete_property(JSValue key) {
  return delete_property_impl(key);
}

ErrorOr<bool> JSObject::delete_property_impl(JSValue key) {
  assert(key.is_atom() || key.is_symbol());
  auto *desc = get_own_property(key);
  if (desc == nullptr) return true;

  if (desc->flag.configurable) {
    storage.erase(JSObjectKey(key));
    return true;
  } else {
    return false;
  }
}

ErrorOr<bool> JSObject::set_property_impl(NjsVM& vm, JSValue key, JSValue value) {
  auto comp = js_to_property_key(vm, key);
  if (comp.is_throw()) return comp.get_value();
  return set_prop(vm, comp.get_value(), value);
}

Completion JSObject::has_own_property(NjsVM& vm, JSValue key) {
  JSValue k = TRY_COMP(js_to_property_key(vm, key));
  return JSValue(has_own_property_atom(k));
}

bool JSObject::set_proto(JSValue proto) {
  assert(proto.is_object() || proto.is_null());

  if (proto.is_null() && _proto_.is_null()) return true;
  if (proto.is_object() && _proto_.is_object()
      && proto.as_object() == _proto_.as_object()) {
    return true;
  }
  if (not extensible) return false;
  JSObject *p = proto.as_object_or_null();

  // check if there is a circular prototype chain
  while (p) {
    if (p == this) return false;
    p = p->get_proto().as_object_or_null();
  }

  _proto_ = proto;
  return true;
}

ErrorOr<bool> JSObject::define_own_property(JSValue key, JSPropDesc& desc) {
  return define_own_property_impl(key, get_own_property(key), desc);
}

ErrorOr<bool> JSObject::define_own_property_impl(JSValue key, JSPropDesc *curr_desc,
                                                 JSPropDesc& desc) {
  assert(desc.flag.in_def_mode);

  auto setup_getset = [] (JSPropDesc& target, JSPropDesc& desc) {
    bool has_getter = desc.flag.has_getter;
    target.flag.has_getter = has_getter;
    target.data.getset.getter = has_getter ? desc.data.getset.getter : undefined;

    bool has_setter = desc.flag.has_setter;
    target.flag.has_setter = has_setter;
    target.data.getset.setter = has_setter ? desc.data.getset.setter : undefined;
  };

  if (curr_desc == nullptr) [[likely]] {
    if (!extensible) return false;

    JSPropDesc& new_desc = storage[JSObjectKey(key)];

    if (desc.is_data_descriptor() || desc.is_generic_descriptor()) {
      new_desc.flag.has_value = true;
      new_desc.data.value = desc.flag.has_value ? desc.data.value : undefined;
      new_desc.flag.writable = desc.flag.has_write & desc.flag.writable;
    }
    else { // desc must be an accessor Property Descriptor
      setup_getset(new_desc, desc);
    }
    new_desc.flag.configurable = desc.flag.has_config & desc.flag.configurable;
    new_desc.flag.enumerable = desc.flag.has_enum & desc.flag.enumerable;

    return true;
  } // end curr_desc == nullptr
  else {
    if (desc.flag.is_empty()) return true;
    if (desc == *curr_desc) return true;

    if (not curr_desc->flag.configurable) {
      if (desc.flag.has_config & desc.flag.configurable) return false;
      if (desc.flag.has_enum && (desc.flag.enumerable != curr_desc->flag.enumerable)) {
        return false;
      }
    }

    if (desc.is_generic_descriptor()) {
      // no further validation is required
    } else if (desc.is_data_descriptor() != curr_desc->is_data_descriptor()) {
      if (not curr_desc->flag.configurable) return false;
      // convert data desc to accessor desc
      if (curr_desc->is_data_descriptor()) {
        curr_desc->flag.has_value = false;
        setup_getset(*curr_desc, desc);
      }
      // convert accessor desc to data desc
      else {
        curr_desc->flag.has_value = true;
        curr_desc->flag.has_getter = false;
        curr_desc->flag.has_setter = false;
        curr_desc->data.value = desc.flag.has_value ? desc.data.value : undefined;
        curr_desc->flag.writable = desc.flag.has_write & desc.flag.writable;
      }
      if (desc.flag.has_enum) curr_desc->flag.enumerable = desc.flag.enumerable;
      if (desc.flag.has_config) curr_desc->flag.configurable = desc.flag.configurable;

    } else if (curr_desc->is_data_descriptor()) {
      if (not curr_desc->flag.configurable) {
        if (not curr_desc->flag.writable && (desc.flag.has_write & desc.flag.writable)) {
          return false;
        }
        if (not curr_desc->flag.writable) {
          if ((desc.flag.has_value && not same_value(desc.data.value, curr_desc->data.value))) {
            return false;
          }
        }
      }
      if (desc.flag.has_value) {
        curr_desc->flag.has_value = true;
        curr_desc->data.value = desc.data.value;
      }
    } else if (curr_desc->is_accessor_descriptor()) {
      if (not curr_desc->flag.configurable) {
        if (desc.flag.has_getter
            && !same_value(desc.data.getset.getter, curr_desc->data.getset.getter)) {
          return false;
        }
        if (desc.flag.has_setter
            && !same_value(desc.data.getset.setter, curr_desc->data.getset.setter)) {
          return false;
        }
      }
      if (desc.flag.has_getter) {
        curr_desc->flag.has_getter = true;
        curr_desc->data.getset.getter = desc.data.getset.getter;
      }
      if (desc.flag.has_setter) {
        curr_desc->flag.has_setter = true;
        curr_desc->data.getset.setter = desc.data.getset.setter;
      }
    }
    // For each field of Desc that is present,
    // set the corresponding attribute of the property named P of object O to the value of the field.
    curr_desc->flag.in_def_mode = false;
    if (desc.flag.has_write) curr_desc->flag.writable = desc.flag.writable;
    if (desc.flag.has_enum) curr_desc->flag.enumerable = desc.flag.enumerable;
    if (desc.flag.has_config) curr_desc->flag.configurable = desc.flag.configurable;

    return true;
  } // end curr_desc != nullptr
}

ErrorOr<bool> JSObject::set_prop(NjsVM& vm, JSValue key, JSValue value) {
  assert(key.is_atom() || key.is_symbol());
  // 1.
  JSPropDesc own_desc;
  if (auto *p = get_exist_prop(key); p == nullptr) {
    own_desc.flag = PFlag::VECW;
    own_desc.to_definition();
    own_desc.data.value = undefined;
  } else {
    own_desc = *p;
  }
  // 2.
  if (own_desc.is_data_descriptor()) {
    if (not own_desc.flag.writable) return false;
    JSPropDesc *existing_desc = get_own_property(key);
    // 2.d
    if (existing_desc) {
      if (existing_desc->is_accessor_descriptor()) return false;
      if (not existing_desc->flag.writable) return false;

      // TODO: looks like this is good enough. Do we really need to call `define_own_property` ?
      existing_desc->data.value = value;
      return true;
//      return define_own_property_impl(key, existing_desc, desc);
    }
    // 2.e
    else {
      // CreateDataProperty
      JSPropDesc desc {
          .flag = PFlag::VECW,
          .data.value = value,
      };
      desc.to_definition();
      return define_own_property_impl(key, existing_desc, desc);
    }
  }
  // is not a data descriptor
  else {
    assert(own_desc.is_accessor_descriptor());
    JSValue setter = own_desc.data.getset.setter;
    if (setter.is_undefined()) return false;

    auto comp = vm.call_function(setter.u.as_func, JSValue(this), nullptr, {value});
    return likely(comp.is_normal()) ? ErrorOr<bool>(true) : comp.get_value();
  }
}

ErrorOr<bool> JSObject::set_prop(NjsVM& vm, u16string_view key_str, JSValue value) {
  return set_prop(vm, JSAtom(vm.str_to_atom(key_str)), value);
}

bool JSObject::add_prop_trivial(NjsVM& vm, u16string_view key_str, JSValue value, PFlag flag) {
  return add_prop_trivial(vm.str_to_atom(key_str), value, flag);
}

bool JSObject::add_prop_trivial(u32 key_atom, JSValue value, PFlag flag) {
  return add_prop_trivial(JSAtom(key_atom), value, flag);
}

bool JSObject::add_prop_trivial(JSValue key, JSValue value, PFlag flag) {
  JSPropDesc& prop = storage[JSObjectKey(key)];
  prop.flag = flag;
  prop.data.value = value;
  return true;
}

bool JSObject::add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl, PFlag flag) {
  u32 name_idx = vm.str_to_atom(key_str);
  JSFunctionMeta meta = build_func_meta(funcImpl);
  return add_prop_trivial(name_idx, JSValue(vm.new_function(meta)), flag);
}

bool JSObject::add_symbol_method(NjsVM& vm, u32 symbol, NativeFuncType funcImpl, PFlag flag) {
  JSFunctionMeta meta = build_func_meta(funcImpl);
  return add_prop_trivial(JSSymbol(symbol), JSValue(vm.new_function(meta)), flag);
}

Completion JSObject::get_prop(NjsVM& vm, u32 key_atom) {
  return get_prop(vm, JSAtom(key_atom));
}

Completion JSObject::get_prop(NjsVM& vm, u16string_view key_str) {
  return get_prop(vm, JSAtom(vm.str_to_atom(key_str)));
}

Completion JSObject::get_prop(NjsVM& vm, JSValue key) {
  assert(key.is_atom() || key.is_symbol());
  JSPropDesc *prop = get_exist_prop(key);
  if (prop == nullptr) [[unlikely]] {
    return prop_not_found;
  } else {
    if (prop->flag.is_value()) [[unlikely]] {
      return prop->data.value;
    } else if (prop->flag.has_getter) {
      JSValue getter = prop->data.getset.getter;
      assert(not getter.is_undefined());
      return vm.call_function(getter.u.as_func, JSValue(this), nullptr, {});
    } else {
      return prop_not_found;
    }
  }
}

Completion JSObject::has_property(NjsVM& vm, JSValue key) {
  if (not key.is_atom()) {
    key = TRY_COMP(js_to_property_key(vm, key));
  }
  return JSValue(get_exist_prop(key) != nullptr);
}

ErrorOr<JSPropDesc> JSObject::to_property_descriptor(NjsVM& vm) {
    JSPropDesc desc;
    desc.flag.in_def_mode = true;

    if (has_prop(AtomPool::k_enumerable)) {
      desc.flag.has_enum = true;
      auto res = TRY_ERR(get_prop(vm, AtomPool::k_enumerable));
      desc.flag.enumerable = res.bool_value();
    }

    if (has_prop(AtomPool::k_configurable)) {
      desc.flag.has_config = true;
      auto res = TRY_ERR(get_prop(vm, AtomPool::k_configurable));
      desc.flag.configurable = res.bool_value();
    }

    if (has_prop(AtomPool::k_value)) {
      desc.flag.has_value = true;
      desc.data.value = TRY_ERR(get_prop(vm, AtomPool::k_value));
    }

    if (has_prop(AtomPool::k_writable)) {
      desc.flag.has_write = true;
      auto res = TRY_ERR(get_prop(vm, AtomPool::k_writable));
      desc.flag.writable = res.bool_value();
    }

    // TODO: check whether the getter is callable.
    if (has_prop(AtomPool::k_get)) {
      desc.flag.has_getter = true;
      auto res = TRY_ERR(get_prop(vm, AtomPool::k_get));
      desc.data.getset.getter = res;
    }

    if (has_prop(AtomPool::k_set)) {
      desc.flag.has_setter = true;
      auto res = TRY_ERR(get_prop(vm, AtomPool::k_set));
      desc.data.getset.setter = res;
    }

    if (desc.is_data_descriptor() && desc.is_accessor_descriptor()) {
      return vm.build_error_internal(JS_TYPE_ERROR, u"A property descriptor can only be either a"
                                                    " data descriptor or an accessor descriptor");
    }

    return desc;
}

Completion JSObject::to_primitive(NjsVM& vm, ToPrimTypeHint preferred_type) {
  JSValue exotic_to_prim = get_prop(vm, AtomPool::k_toPrimitive).get_value();
  if (exotic_to_prim.is_function()) {
    JSValue hint_arg;
    if (preferred_type == HINT_DEFAULT) {
      hint_arg = vm.new_primitive_string(u"default");
    } else if (preferred_type == HINT_NUMBER) {
      hint_arg = vm.get_string_const(AtomPool::k_number);
    } else {
      hint_arg = vm.get_string_const(AtomPool::k_string);
    }
    Completion to_prim_res = vm.call_function(
        exotic_to_prim.u.as_func, JSValue(this), nullptr, {hint_arg});
    if (to_prim_res.is_throw()) {
      return to_prim_res;
    }
    else if (to_prim_res.get_value().is_object()) {
      JSValue err = vm.build_error_internal(JS_TYPE_ERROR, u"");
      return CompThrow(err);
    }
    else {
      return to_prim_res;
    }
  }
  else {
    return ordinary_to_primitive(vm, HINT_NUMBER);
  }

}

Completion JSObject::ordinary_to_primitive(NjsVM& vm, ToPrimTypeHint hint) {
  std::array<u32, 2> method_names_atom;
  if (hint == HINT_STRING) {
    method_names_atom = {AtomPool::k_toString, AtomPool::k_valueOf};
  } else if (hint == HINT_NUMBER) {
    method_names_atom = {AtomPool::k_valueOf, AtomPool::k_toString};
  }

  for (u32 method_atom : method_names_atom) {
    JSValue method = get_prop(vm, method_atom).get_value();
    if (not method.is_function()) continue;

    Completion comp = vm.call_function(method.u.as_func, JSValue(this), nullptr, {});
    if (comp.is_throw()) return comp;

    if (not comp.get_value().is_object()) {
      return comp;
    }
  }

  JSValue err = vm.build_error_internal(JS_TYPE_ERROR, u"");
  return CompThrow(err);
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