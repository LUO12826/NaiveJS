#ifndef NJS_JS_SYMBOL_H
#define NJS_JS_SYMBOL_H

#include "RCObject.h"
#include "njs/utils/helper.h"
#include <string>

namespace njs {

/// @brief https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol
struct JSSymbol: public RCObject {

  inline static size_t global_count {0};

  JSSymbol() {}
  explicit JSSymbol(std::u16string name);

  RCObject *copy() override {
    auto *sym = new JSSymbol();
    sym->seq = this->seq;
    sym->name = this->name;
    return sym;
  }

  bool operator == (const JSSymbol& other) const;

  std::string to_string();

  std::u16string name;
  size_t seq;
};

inline JSSymbol::JSSymbol(std::u16string name): name(std::move(name)) {
  seq = JSSymbol::global_count;
  JSSymbol::global_count += 1;
}

inline bool JSSymbol::operator == (const JSSymbol& other) const {
  return name == other.name && seq == other.seq;
}

inline std::string JSSymbol::to_string() {
  return "JSSymbol(" + to_u8string(name) + ")";
}

} // namespace njs

/// @brief std::hash for JSSymbol. Injected into std namespace.
template <>
struct std::hash<njs::JSSymbol>
{
  std::size_t operator () (const njs::JSSymbol& symbol) const {
    return njs::hash_val(symbol.name, symbol.seq);
  }
};

#endif //NJS_JS_SYMBOL_H
