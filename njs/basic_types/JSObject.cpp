#include "JSObject.h"

#include "njs/gc/GCHeap.h"
#include "JSValue.h"

namespace njs {

JSObjectKey::~JSObjectKey() {
  if (key_type == KEY_STR) key.str.release();
  if (key_type == KEY_SYMBOL) key.symbol.release();
}

bool JSObjectKey::operator == (const JSObjectKey& other) const {
  if (key_type != other.key_type) return false;
  if (key_type == KEY_STR) return key.str == other.key.str;
  if (key_type == KEY_NUM) return key.number == other.key.number;
  if (key_type == KEY_SYMBOL) return key.symbol == other.key.symbol;

  __builtin_unreachable();
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