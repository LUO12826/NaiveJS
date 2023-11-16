#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"
#include "njs/vm/NjsVM.h"
#include <sstream>

namespace njs {

JSObjectKey::JSObjectKey(JSSymbol *sym): key_type(KEY_SYMBOL) {
  key.symbol = sym;
  sym->retain();
}

JSObjectKey::JSObjectKey(PrimitiveString *str): key_type(KEY_STR) {
  key.str = str;
  str->retain();
}

JSObjectKey::JSObjectKey(int64_t atom): key_type(KEY_ATOM) {
  key.atom = atom;
}

JSObjectKey::~JSObjectKey() {
  if (key_type == KEY_STR) key.str->release();
  if (key_type == KEY_SYMBOL) key.symbol->release();
}

bool JSObjectKey::operator == (const JSObjectKey& other) const {
  if (key_type != other.key_type) {
    return false;
  }
  if (key_type == KEY_STR) return *(key.str) == *(other.key.str);
  if (key_type == KEY_NUM) return key.number == other.key.number;
  if (key_type == KEY_SYMBOL) return *(key.symbol) == *(other.key.symbol);
  if (key_type == KEY_ATOM) return key.atom == other.key.atom;

  __builtin_unreachable();
}

std::string JSObjectKey::to_string() const {
  if (key_type == KEY_STR) return to_u8string(key.str->str);
  if (key_type == KEY_NUM) return std::to_string(key.number);
  if (key_type == KEY_SYMBOL) return key.symbol->to_string();
  if (key_type == KEY_ATOM) return "Atom(" + std::to_string(key.atom) + ")";

  __builtin_unreachable();
}

bool JSObject::add_prop(const JSValue& key, const JSValue& value) {
  JSValue *val_placeholder = nullptr;
  if (key.tag == JSValue::JS_ATOM) {
    val_placeholder = &storage[JSObjectKey(key.val.as_int64)];
  }
  else if (key.tag == JSValue::STRING) {
    val_placeholder = &storage[JSObjectKey(key.val.as_primitive_string)];
  }
  else if (key.tag == JSValue::SYMBOL) {
    val_placeholder = &storage[JSObjectKey(key.val.as_symbol)];
  }
  else if (key.tag == JSValue::NUM_FLOAT) {
    val_placeholder = &storage[JSObjectKey(key.val.as_float64)];
  }
  else if (key.tag == JSValue::NUM_INT) {
    val_placeholder = &storage[JSObjectKey(key.val.as_int64)];
  }
  else {
    return false;
  }

  val_placeholder->assign(value);
  return true;
}

bool JSObject::add_prop(int64_t key_atom, const JSValue& value) {
  storage[JSObjectKey(key_atom)].assign(value);
  return true;
}

bool JSObject::add_prop(NjsVM& vm, u16string_view key_str, const JSValue& value) {
  u32 key_idx = vm.str_pool.add_string(key_str);
  storage[JSObjectKey(key_idx)].assign(value);
  return true;
}

JSValue JSObject::get_prop(NjsVM& vm, u16string_view name) {
  return get_prop(vm, name, false);
}

JSValue JSObject::get_prop(NjsVM& vm, u16string_view key_str, bool get_ref) {
  int64_t key_idx = vm.str_pool.add_string(key_str);
  return get_prop(key_idx, get_ref);
}

bool JSObject::add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl) {

  u32 name_idx = vm.str_pool.add_string(key_str);
  JSValue key(JSValue::JS_ATOM);
  key.val.as_int64 = name_idx;

  JSFunctionMeta meta {
      .name_index = name_idx,
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = funcImpl,
  };

  return add_prop(key, JSValue(vm.new_function(meta)));
}

void JSObject::gc_scan_children(GCHeap& heap) {
  for (auto& [key, value]: storage) {
    if (value.needs_gc()) {
      heap.gc_visit_object(value, value.as_GCObject());
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
  for (auto& [key, value] : storage) {
    stream << key.to_string() << ": ";
    stream << value.description() << ", ";

    i += 1;
    if (i == print_prop_num) break;
  }

  stream << "}";
  return stream.str();
}

std::string JSObject::to_string(NjsVM& vm) {
  std::string output = "{ ";

  for (auto& [key, value] : storage) {
    if (key.key_type == JSObjectKey::KEY_ATOM) {
      output += to_u8string(vm.str_pool.get_string(key.key.atom));
    }
    else assert(false);

    output += ": ";
    if (value.tag_is(JSValue::STRING)) output += '\'';
    output += value.to_string(vm);
    if (value.tag_is(JSValue::STRING)) output += '\'';
    output += ", ";
  }

  output += "}";
  return output;
}

void JSObject::to_json(u16string& output, NjsVM& vm) const {
  output += u"{";

  bool first = true;
  for (auto& [key, value] : storage) {
    if (value.tag_is(JSValue::UNDEFINED)) continue;
    if (first) first = false;
    else output += u',';
    
    if (key.key_type == JSObjectKey::KEY_ATOM) {
      output += u'"';
      output += vm.str_pool.get_string(key.key.atom);
      output += u'"';
    }
    else assert(false);

    output += u':';
    value.to_json(output, vm);
  }

  output += u"}";
}

JSObject::~JSObject() {
  for (auto& [key, value] : storage) {
    if (value.is_RCObject()) value.val.as_RCObject->release();
  }
}

}