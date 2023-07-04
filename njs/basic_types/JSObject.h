#ifndef NJS_JSOBJECT_H
#define NJS_JSOBJECT_H

#include <cstdint>
#include <string>

#include "njs/include/robin_hood.h"
#include "njs/utils/helper.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCObject.h"
#include "RCObject.h"


namespace njs {

using std::u16string;
using robin_hood::unordered_map;

class GCHeap;

struct JSObjectKey {
  enum KeyType {
    KEY_STR,
    KEY_NUM,
    KEY_SYMBOL,
  };

  union KeyData {
    PrimitiveString str;
    JSSymbol symbol;
    double number;

    KeyData(): number(0) {}
    ~KeyData() {}
  };

  ~JSObjectKey();

  bool operator == (const JSObjectKey& other) const;

  KeyData key;
  
  KeyType key_type;
};

} // namespace njs


/// @brief std::hash for JSObjectKey. Injected into std namespace.
template <>
struct std::hash<njs::JSObjectKey>
{
  std::size_t operator () (const njs::JSObjectKey& obj_key) const {
    switch (obj_key.key_type) {
      case njs::JSObjectKey::KEY_STR: return njs::hash_val(obj_key.key_type, obj_key.key.str);
      case njs::JSObjectKey::KEY_NUM: return njs::hash_val(obj_key.key_type, obj_key.key.number);
      case njs::JSObjectKey::KEY_SYMBOL: return njs::hash_val(obj_key.key_type, obj_key.key.symbol);
      default: return 0;
    }
  }
};

namespace njs {

class JSObject : public GCObject {
 public:
  JSObject() : GCObject(ObjectClass::CLS_OBJECT) {}
  explicit JSObject(ObjectClass cls) : GCObject(cls) {}

  void gc_scan_children(GCHeap& heap) override;

  unordered_map<JSObjectKey, JSValue> storage;
};

} // namespace njs

#endif // NJS_JSOBJECT_H