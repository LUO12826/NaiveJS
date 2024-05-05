#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"
#include "njs/vm/NjsVM.h"
#include <sstream>
#include <array>

namespace njs {

Completion JSObject::to_primitive(NjsVM& vm, u16string_view preferred_type) {
  JSValue exotic_to_prim = get_prop(StringPool::ATOM_toPrimitive, false);
  if (exotic_to_prim.is_function()) {
    JSValue hint_arg(new PrimitiveString(u16string(preferred_type)));
    Completion to_prim_res = vm.call_function(exotic_to_prim.val.as_function, {hint_arg}, this);
    if (to_prim_res.is_throw()) {
      return to_prim_res;
    }
    else if (to_prim_res.get_value().is_object()) {
      JSValue err = InternalFunctions::build_error_internal(vm, u"TypeError");
      return Completion::with_throw(err);
    }
    else {
      return to_prim_res;
    }
  }
  else {
    return ordinary_to_primitive(vm, preferred_type);
  }

}

Completion JSObject::ordinary_to_primitive(NjsVM& vm, u16string_view hint) {
  std::array<int64_t, 2> method_names_atom;
  if (hint == u"string") {
    method_names_atom = {StringPool::ATOM_toString, StringPool::ATOM_valueOf};
  } else if (hint == u"number") {
    method_names_atom = {StringPool::ATOM_valueOf, StringPool::ATOM_toString};
  }

  for (int64_t method_atom : method_names_atom) {
    JSValue method = get_prop(method_atom, false);
    if (not method.is_function()) continue;

    Completion comp = vm.call_function(method.val.as_function, {}, this);
    if (comp.is_throw()) return comp;

    if (not comp.get_value().is_object()) {
      return comp;
    }
  }

  JSValue err = InternalFunctions::build_error_internal(vm, u"TypeError");
  return Completion::with_throw(err);
}

bool JSObject::add_prop(const JSValue& key, const JSValue& value, PropDesc desc) {
  JSObjectProp *new_prop;
  if (likely(key.tag == JSValue::JS_ATOM)) {
    new_prop = &storage[JSObjectKey(key.val.as_i64)];
  } else if (key.tag == JSValue::SYMBOL) {
    new_prop = &storage[JSObjectKey(key.val.as_symbol)];
  } else {
    return false;
  }

  new_prop->data.value.assign(value);
  new_prop->desc = desc;
  return true;
}

bool JSObject::add_prop(int64_t key_atom, const JSValue& value, PropDesc desc) {
  JSObjectProp& prop = storage[JSObjectKey(key_atom)];
  prop.data.value.assign(value);
  prop.desc = desc;
  return true;
}

bool JSObject::add_prop(NjsVM& vm, u16string_view key_str, const JSValue& value, PropDesc desc) {
  return add_prop(vm.sv_to_atom(key_str), value, desc);
}

JSValue JSObject::get_prop(NjsVM& vm, u16string_view name) {
  return get_prop(vm, name, false);
}

JSValue JSObject::get_prop(NjsVM& vm, u16string_view key_str, bool get_ref) {
  return get_prop(vm.sv_to_atom(key_str), get_ref);
}

bool JSObject::add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl) {

  u32 name_idx = vm.sv_to_atom(key_str);

  JSFunctionMeta meta {
      .name_index = name_idx,
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = funcImpl,
  };

  return add_prop((int64_t)name_idx, JSValue(vm.new_function(meta)));
}

void JSObject::gc_scan_children(GCHeap& heap) {
  for (auto& [key, prop]: storage) {
    if (prop.desc.is_value() && prop.data.value.needs_gc()) {
      heap.gc_visit_object(prop.data.value, prop.data.value.as_GCObject());
    }
    if (prop.desc.is_getset()) {
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
    if (not prop.desc.is_enumerable()) continue;

    stream << key.to_string() << ": ";
    stream << prop.data.value.description() << ", ";

    i += 1;
    if (i == print_prop_num) break;
  }

  stream << "}";
  return stream.str();
}

std::string JSObject::to_string(NjsVM& vm) {
  std::string output = "{ ";

  for (auto& [key, prop] : storage) {
    if (not prop.desc.is_enumerable()) continue;

    if (key.key_type == JSObjectKey::KEY_ATOM) {
      u16string escaped = to_escaped_u16string(vm.atom_to_str(key.key.atom));
      output += to_u8string(escaped);
    }
    else assert(false);

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
    if (not prop.desc.is_enumerable()) continue;
    if (prop.data.value.is_undefined()) continue;

    if (first) first = false;
    else output += u',';
    
    if (key.key_type == JSObjectKey::KEY_ATOM) {
      output += u'"';
      output += to_escaped_u16string(vm.atom_to_str(key.key.atom));
      output += u'"';
    }
    else assert(false);

    output += u':';
    prop.data.value.to_json(output, vm);
  }

  output += u"}";
}

JSObject::~JSObject() {
  for (auto& [key, prop] : storage) {
    if (prop.data.value.is_RCObject()) prop.data.value.val.as_RCObject->release();
  }
}

}