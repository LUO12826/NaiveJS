#ifndef NJS_JSOBJECT_KEY_H
#define NJS_JSOBJECT_KEY_H

#include <string>

#include "JSSymbol.h"
#include "JSValue.h"
#include "njs/utils/helper.h"
#include "njs/utils/macros.h"

namespace njs {

using std::u16string;
using std::u16string_view;

struct PrimitiveString;
struct JSSymbol;

struct JSObjectKey {
  enum KeyType {
    KEY_SYMBOL,
    KEY_ATOM,
  };

  union KeyData {
    JSSymbol *symbol;
    int64_t atom;

    KeyData() {}
    ~KeyData() {}
  };

  explicit JSObjectKey(JSSymbol *sym);
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

inline JSObjectKey::JSObjectKey(int64_t atom): key_type(KEY_ATOM) {
  key.atom = atom;
}

inline JSObjectKey::~JSObjectKey() {
  if (key_type == KEY_SYMBOL) key.symbol->release();
}

inline bool JSObjectKey::operator == (const JSObjectKey& other) const {
  if (key_type != other.key_type) {
    return false;
  }
  if (key_type == KEY_SYMBOL) return *(key.symbol) == *(other.key.symbol);
  if (key_type == KEY_ATOM) return key.atom == other.key.atom;

  __builtin_unreachable();
}

inline std::string JSObjectKey::to_string() const {
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
    if (likely(obj_key.key_type == njs::JSObjectKey::KEY_ATOM)) {
      return obj_key.key.atom;
    } else {
      return njs::hash_val(obj_key.key_type, *(obj_key.key.symbol));
    }
  }
};


#endif //NJS_JSOBJECT_KEY_H
