#ifndef NJS_JSOBJECT_KEY_H
#define NJS_JSOBJECT_KEY_H

#include <string>

#include "JSSymbol.h"
#include "JSValue.h"
#include "njs/utils/helper.h"

namespace njs {

using std::u16string;
using std::u16string_view;

struct PrimitiveString;
struct JSSymbol;

struct JSObjectKey {
  enum KeyType {
    KEY_STR,
    KEY_NUM,
    KEY_SYMBOL,
    KEY_ATOM,
  };

  union KeyData {
    PrimitiveString *str;
    JSSymbol *symbol;
    double number;
    int64_t atom;

    KeyData(): number(0) {}
    ~KeyData() {}
  };

  explicit JSObjectKey(JSSymbol *sym);
  explicit JSObjectKey(PrimitiveString *str);
  explicit JSObjectKey(int64_t atom);

  ~JSObjectKey();

  bool operator == (const JSObjectKey& other) const;
  std::string to_string() const;

  KeyData key;
  KeyType key_type;
};

inline JSObjectKey::JSObjectKey(JSSymbol *sym): key_type(KEY_SYMBOL) {
  key.symbol = sym;
  sym->retain();
}

inline JSObjectKey::JSObjectKey(PrimitiveString *str): key_type(KEY_STR) {
  key.str = str;
  str->retain();
}

inline JSObjectKey::JSObjectKey(int64_t atom): key_type(KEY_ATOM) {
  key.atom = atom;
}

inline JSObjectKey::~JSObjectKey() {
  if (key_type == KEY_STR) key.str->release();
  if (key_type == KEY_SYMBOL) key.symbol->release();
}

inline bool JSObjectKey::operator == (const JSObjectKey& other) const {
  if (key_type != other.key_type) {
    return false;
  }
  if (key_type == KEY_STR) return *(key.str) == *(other.key.str);
  if (key_type == KEY_NUM) return key.number == other.key.number;
  if (key_type == KEY_SYMBOL) return *(key.symbol) == *(other.key.symbol);
  if (key_type == KEY_ATOM) return key.atom == other.key.atom;

  __builtin_unreachable();
}

inline std::string JSObjectKey::to_string() const {
  if (key_type == KEY_STR) return to_u8string(key.str->str);
  if (key_type == KEY_NUM) return std::to_string(key.number);
  if (key_type == KEY_SYMBOL) return key.symbol->to_string();
  if (key_type == KEY_ATOM) return "Atom(" + std::to_string(key.atom) + ")";

  __builtin_unreachable();
}

}

/// @brief std::hash for JSObjectKey. Injected into std namespace.
template <>
struct std::hash<njs::JSObjectKey>
{
  std::size_t operator () (const njs::JSObjectKey& obj_key) const {
    switch (obj_key.key_type) {
      case njs::JSObjectKey::KEY_STR:
        return njs::hash_val(njs::JSObjectKey::KEY_STR, *(obj_key.key.str));
      case njs::JSObjectKey::KEY_NUM:
        return njs::hash_val(obj_key.key_type, obj_key.key.number);
      case njs::JSObjectKey::KEY_SYMBOL:
        return njs::hash_val(obj_key.key_type, *(obj_key.key.symbol));
      case njs::JSObjectKey::KEY_ATOM:
        return obj_key.key.atom;
      default: assert(false);
    }
  }
};


#endif //NJS_JSOBJECT_KEY_H
