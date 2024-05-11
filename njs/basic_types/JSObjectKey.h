#ifndef NJS_JSOBJECT_KEY_H
#define NJS_JSOBJECT_KEY_H

#include <string>
#include "JSValue.h"
#include "njs/utils/macros.h"

namespace njs {

using std::u16string;
using std::u16string_view;

struct PrimitiveString;

struct JSObjectKey {
  explicit JSObjectKey(u32 atom);
  explicit JSObjectKey(JSValue val);

  bool operator == (const JSObjectKey& other) const;
  std::string to_string() const;

  JSValue key;
};

inline JSObjectKey::JSObjectKey(u32 atom): key(JSValue::JS_ATOM) {
  key.val.as_atom = atom;
}

inline JSObjectKey::JSObjectKey(JSValue val): key(val) {
  assert(val.is(JSValue::SYMBOL) || val.is(JSValue::JS_ATOM));
}

inline bool JSObjectKey::operator == (const JSObjectKey& other) const {
  return key.val.as_atom == other.key.val.as_atom;
}

inline std::string JSObjectKey::to_string() const {
  if (key.is(JSValue::JS_ATOM)) {
    return "Atom(" + std::to_string(key.val.as_atom) + ")";
  } else {
    return "Symbol(" + std::to_string(key.val.as_symbol) + ")";
  }
}

}

/// @brief std::hash for JSObjectKey. Injected into std namespace.
template <>
struct std::hash<njs::JSObjectKey> {
  std::size_t operator () (const njs::JSObjectKey& obj_key) const {
    return obj_key.key.val.as_atom;
  }
};


#endif //NJS_JSOBJECT_KEY_H
