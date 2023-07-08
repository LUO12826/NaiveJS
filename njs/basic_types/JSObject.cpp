#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"
#include "njs/common/enum_strings.h"
#include <sstream>

namespace njs {

JSObjectKey::JSObjectKey(JSSymbol *sym): key_type(KEY_SYMBOL) {
  key.symbol = sym;
  sym->retain();
}

JSObjectKey::JSObjectKey(double num): key_type(KEY_NUM) {
  key.number = num;
}

JSObjectKey::JSObjectKey(PrimitiveString *str): key_type(KEY_STR) {
  key.str = str;
  str->retain();
}

JSObjectKey::JSObjectKey(u16string_view str_view): key_type(KEY_STR_VIEW) {
  key.str_view = str_view;
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
    if (key_type == KEY_STR && other.key_type == KEY_STR_VIEW) {
      return (key.str)->str == other.key.str_view;
    }
    else if (key_type == KEY_STR_VIEW && other.key_type == KEY_STR) {
      return key.str_view == other.key.str->str;
    }
    return false;
  }
  if (key_type == KEY_STR) return *(key.str) == *(other.key.str);
  if (key_type == KEY_NUM) return key.number == other.key.number;
  if (key_type == KEY_SYMBOL) return *(key.symbol) == *(other.key.symbol);
  if (key_type == KEY_ATOM) return key.atom == other.key.atom;

  __builtin_unreachable();
}

std::string JSObjectKey::to_string() {
  if (key_type == KEY_STR) return to_utf8_string(key.str->str);
  if (key_type == KEY_STR_VIEW) return to_utf8_string(key.str_view);
  if (key_type == KEY_NUM) return std::to_string(key.number);
  if (key_type == KEY_SYMBOL) return key.symbol->to_string();
  if (key_type == KEY_ATOM) return "Atom(" + std::to_string(key.atom) + ")";

  __builtin_unreachable();
}

bool JSObject::add_prop(JSValue& key, JSValue& value) {
  JSValue *val_placeholder = nullptr;
  if (key.tag == JSValue::JS_ATOM) {
    val_placeholder = &storage[JSObjectKey(key.val.as_int)];
  }
  else if (key.tag == JSValue::STRING) {
    val_placeholder = &storage[JSObjectKey(key.val.as_primitive_string)];
  }
  else if (key.tag == JSValue::SYMBOL) {
    val_placeholder = &storage[JSObjectKey(key.val.as_symbol)];
  }
  else if (key.tag == JSValue::NUMBER_FLOAT) {
    val_placeholder = &storage[JSObjectKey(key.val.as_float64)];
  }
  else if (key.tag == JSValue::NUMBER_INT) {
    val_placeholder = &storage[JSObjectKey(key.val.as_int)];
  }
  else {
    return false;
  }

  val_placeholder->assign(value);
  return true;
}

JSValue JSObject::get_prop(u16string_view key, bool get_ref) {

  if (!get_ref) {
    auto res = storage.find(JSObjectKey(key));
    if (res != storage.end()) return res->second;
    return JSValue::undefined;
  }
  else {
    return JSValue(&storage[JSObjectKey(key)]);
  }
}

void JSObject::gc_scan_children(GCHeap& heap) {
  for (auto& kv_pair: storage) {
    JSValue& val = kv_pair.second;

    if (val.needs_gc()) {
      heap.gc_visit_object(val, val.as_GCObject());
    }
  }
}

std::string JSObject::description() {
  std::ostringstream stream;
  if (obj_class == ObjectClass::CLS_OBJECT) stream << "{ ";
  if (obj_class == ObjectClass::CLS_FUNCTION) stream << "Function{ ";


  u32 print_prop_num = std::min((u32)4, (u32)storage.size());
  u32 i = 0;
  for (auto& kv_pair : storage) {
    stream << kv_pair.first.to_string() << ": ";
    stream << kv_pair.second.to_string() << ", ";

    i += 1;
    if (i == print_prop_num) break;
  }

  stream << "}";
  return stream.str();
}

}