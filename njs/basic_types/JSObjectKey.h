#ifndef NJS_JSOBJECT_KEY_H
#define NJS_JSOBJECT_KEY_H

#include <cstdint>
#include <string>

#include "njs/utils/helper.h"

namespace njs {

using std::u16string;
using std::u16string_view;

struct PrimitiveString;
struct JSSymbol;

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
  explicit JSObjectKey(PrimitiveString *str);
  explicit JSObjectKey(u16string_view str_view);
  explicit JSObjectKey(int64_t atom);

  ~JSObjectKey();

  bool operator == (const JSObjectKey& other) const;
  std::string to_string() const;

  KeyData key;
  KeyType key_type;
};

}

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


#endif //NJS_JSOBJECT_KEY_H
