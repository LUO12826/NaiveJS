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

  explicit JSObjectKey(JSSymbol *sym);
  explicit JSObjectKey(int64_t atom);

  bool operator == (const JSObjectKey& other) const;
  std::string to_string() const;

  JSValue key;
};

inline JSObjectKey::JSObjectKey(JSSymbol *sym): key(JSValue::SYMBOL) {
  key.val.as_symbol = sym;
}

inline JSObjectKey::JSObjectKey(int64_t atom): key(JSValue::JS_ATOM) {
  key.val.as_i64 = atom;
}

inline bool JSObjectKey::operator == (const JSObjectKey& other) const {
  if (key.tag != other.key.tag) {
    return false;
  }
  if (key.tag == JSValue::JS_ATOM) return key.val.as_i64 == other.key.val.as_i64;
  if (key.tag == JSValue::SYMBOL) return *(key.val.as_symbol) == *(other.key.val.as_symbol);

  __builtin_unreachable();
}

inline std::string JSObjectKey::to_string() const {
  if (key.tag == JSValue::JS_ATOM) return "Atom(" + std::to_string(key.val.as_i64) + ")";
  if (key.tag == JSValue::SYMBOL) return key.val.as_symbol->to_string();

  __builtin_unreachable();
}

}

/// @brief std::hash for JSObjectKey. Injected into std namespace.
template <>
struct std::hash<njs::JSObjectKey>
{
  std::size_t operator () (const njs::JSObjectKey& obj_key) const {
    if (likely(obj_key.key.tag == njs::JSValue::JS_ATOM)) {
      return obj_key.key.val.as_i64;
    } else {
      return njs::hash_val(njs::JSValue::SYMBOL, *(obj_key.key.val.as_symbol));
    }
  }
};


#endif //NJS_JSOBJECT_KEY_H
