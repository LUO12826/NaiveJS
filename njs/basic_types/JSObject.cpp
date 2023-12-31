#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"
#include "njs/vm/NjsVM.h"
#include <sstream>

namespace njs {

bool JSObject::add_prop(const JSValue& key, const JSValue& value, bool enumerable, bool configurable, bool writable) {
  PropDesc *new_prop;
  if (key.tag == JSValue::JS_ATOM) {
    new_prop = &storage[JSObjectKey(key.val.as_int64)];
  }
  else if (key.tag == JSValue::STRING) {
    new_prop = &storage[JSObjectKey(key.val.as_primitive_string)];
  }
  else if (key.tag == JSValue::SYMBOL) {
    new_prop = &storage[JSObjectKey(key.val.as_symbol)];
  }
  else if (key.tag == JSValue::NUM_FLOAT) {
    new_prop = &storage[JSObjectKey(key.val.as_float64)];
  }
  else {
    return false;
  }

  new_prop->value.assign(value);
  new_prop->enumerable = enumerable;
  new_prop->configurable = configurable;
  new_prop->writable = writable;
  return true;
}

bool JSObject::add_prop(int64_t key_atom, const JSValue& value, bool enumerable, bool configurable, bool writable) {
  PropDesc& prop = storage[JSObjectKey(key_atom)];
  prop.value.assign(value);
  prop.enumerable = enumerable;
  prop.configurable = configurable;
  prop.writable = writable;
  return true;
}

bool JSObject::add_prop(NjsVM& vm, u16string_view key_str, const JSValue& value, bool enumerable, bool configurable, bool writable) {
  u32 key_atom = vm.str_pool.add_string(key_str);
  return add_prop((int64_t)key_atom, value, enumerable, configurable, writable);
}

JSValue JSObject::get_prop(NjsVM& vm, u16string_view name) {
  return get_prop(vm, name, false);
}

JSValue JSObject::get_prop(NjsVM& vm, u16string_view key_str, bool get_ref) {
  int64_t key_atom = vm.str_pool.add_string(key_str);
  return get_prop(key_atom, get_ref);
}

bool JSObject::add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl) {

  u32 name_idx = vm.str_pool.add_string(key_str);

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
    if (prop.value.needs_gc()) {
      heap.gc_visit_object(prop.value, prop.value.as_GCObject());
    }
    if (prop.getter.needs_gc()) {
      heap.gc_visit_object(prop.value, prop.value.as_GCObject());
    }
    if (prop.setter.needs_gc()) {
      heap.gc_visit_object(prop.value, prop.value.as_GCObject());
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
    if (not prop.enumerable) continue;

    stream << key.to_string() << ": ";
    stream << prop.value.description() << ", ";

    i += 1;
    if (i == print_prop_num) break;
  }

  stream << "}";
  return stream.str();
}

std::string JSObject::to_string(NjsVM& vm) {
  std::string output = "{ ";

  for (auto& [key, prop] : storage) {
    if (not prop.enumerable) continue;

    if (key.key_type == JSObjectKey::KEY_ATOM) {
      output += to_u8string(vm.str_pool.get_string(key.key.atom));
    }
    else assert(false);

    output += ": ";
    if (prop.value.tag_is(JSValue::STRING)) output += '\'';
    output += prop.value.to_string(vm);
    if (prop.value.tag_is(JSValue::STRING)) output += '\'';
    output += ", ";
  }

  output += "}";
  return output;
}

void JSObject::to_json(u16string& output, NjsVM& vm) const {
  output += u"{";

  bool first = true;
  for (auto& [key, prop] : storage) {
    if (not prop.enumerable) continue;
    if (prop.value.is_undefined()) continue;

    if (first) first = false;
    else output += u',';
    
    if (key.key_type == JSObjectKey::KEY_ATOM) {
      output += u'"';
      output += vm.str_pool.get_string(key.key.atom);
      output += u'"';
    }
    else assert(false);

    output += u':';
    prop.value.to_json(output, vm);
  }

  output += u"}";
}

JSObject::~JSObject() {
  for (auto& [key, prop] : storage) {
    if (prop.value.is_RCObject()) prop.value.val.as_RCObject->release();
  }
}

}