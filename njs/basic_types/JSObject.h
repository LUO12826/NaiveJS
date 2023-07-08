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
using std::u16string_view;
using robin_hood::unordered_map;

class GCHeap;

struct JSObjectKey {
  enum KeyType {
    KEY_STR,
    KEY_STR_VIEW,
    KEY_NUM,
    KEY_SYMBOL,
    KEY_ATOM,
  };

  union KeyData {
    PrimitiveString *str;
    u16string_view str_view;
    JSSymbol *symbol;
    double number;
    int64_t atom;

    KeyData(): number(0) {}
    ~KeyData() {}
  };

  explicit JSObjectKey(JSSymbol *sym);
  explicit JSObjectKey(double num);
  explicit JSObjectKey(PrimitiveString *str);
  explicit JSObjectKey(u16string_view str_view);
  explicit JSObjectKey(int64_t atom);

  ~JSObjectKey();

  bool operator == (const JSObjectKey& other) const;
  std::string to_string();

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
      case njs::JSObjectKey::KEY_STR:
        return njs::hash_val(njs::JSObjectKey::KEY_STR, *(obj_key.key.str));
      case njs::JSObjectKey::KEY_STR_VIEW:
        return njs::hash_val(njs::JSObjectKey::KEY_STR, obj_key.key.str_view);
      case njs::JSObjectKey::KEY_NUM:
        return njs::hash_val(obj_key.key_type, obj_key.key.number);
      case njs::JSObjectKey::KEY_SYMBOL:
        return njs::hash_val(obj_key.key_type, *(obj_key.key.symbol));
      case njs::JSObjectKey::KEY_ATOM:
        return njs::hash_val(obj_key.key_type, obj_key.key.atom);
      default: assert(false);
    }
  }
};

namespace njs {

// has corresponding string representation, note to modify when adding
enum class ObjectClass {
  CLS_OBJECT = 0,
  CLS_ARRAY,
  CLS_ERROR,
  CLS_DATE,
  CLS_FUNCTION,
  CLS_CUSTOM
};

class JSObject : public GCObject {
 public:
  JSObject(): GCObject(sizeof(JSObject)), obj_class(ObjectClass::CLS_OBJECT) {}
  explicit JSObject(ObjectClass cls): GCObject(sizeof(JSObject)), obj_class(cls) {}

  void gc_scan_children(GCHeap& heap) override;
  std::string description() override;

  bool add_prop(JSValue& key, JSValue& value);

  JSValue get_prop(u16string_view key, bool get_ref);

  ObjectClass obj_class;
  unordered_map<JSObjectKey, JSValue> storage;
};

} // namespace njs

#endif // NJS_JSOBJECT_H