#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"

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

JSObjectKey::JSObjectKey(int64_t atom): key_type(KEY_ATOM) {
  key.atom = atom;
}

JSObjectKey::~JSObjectKey() {
  if (key_type == KEY_STR) key.str->release();
  if (key_type == KEY_SYMBOL) key.symbol->release();
}

bool JSObjectKey::operator == (const JSObjectKey& other) const {
  if (key_type != other.key_type) return false;
  if (key_type == KEY_STR) return *(key.str) == *(other.key.str);
  if (key_type == KEY_NUM) return key.number == other.key.number;
  if (key_type == KEY_SYMBOL) return *(key.symbol) == *(other.key.symbol);
  if (key_type == KEY_ATOM) return key.atom == other.key.atom;

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

void JSObject::gc_scan_children(GCHeap& heap) {
  for (auto& kv_pair: storage) {
    JSValue& val = kv_pair.second;

    if (val.needs_gc()) {
      heap.gc_visit_object(val, val.as_GCObject());
    }
  }
}

}