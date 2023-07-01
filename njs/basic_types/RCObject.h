#ifndef NJS_RCOBJECT_H
#define NJS_RCOBJECT_H

#include <cstdint>
#include <string>
#include "njs/utils/helper.h"

namespace njs {

using u32 = uint32_t;

class RCObject {
 public:
  RCObject() {}
  virtual ~RCObject() {}

  RCObject(const RCObject& obj) = delete;
  RCObject(RCObject&& obj) = delete;

  void retain() { ref_count += 1; }

  void release() {
    ref_count -= 1;
    if (ref_count == 0) delete this;
  }

 private:
  u32 ref_count;
};

/// @brief PrimitiveString: string that is not wrapped as objects in JavaScript
struct PrimitiveString: public RCObject {

  PrimitiveString(std::u16string str): str(std::move(str)) {}

  bool operator == (const PrimitiveString& other) const {
    return str == other.str;
  }

  std::u16string str;
};

/// @brief https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol
struct JSSymbol: public RCObject {

  static size_t global_count;

  JSSymbol(std::u16string name): name(std::move(name)) {
    seq = JSSymbol::global_count;
    JSSymbol::global_count += 1;
  }

  bool operator == (const JSSymbol& other) const {
    return name == other.name && seq == other.seq;
  }

  std::u16string name;
  size_t seq;
};

} // namespace njs

/// @brief std::hash for PrimitiveString. Injected into std namespace.
template <>
struct std::hash<njs::PrimitiveString>
{
  std::size_t operator () (const njs::PrimitiveString& str) const {
    return std::hash<decltype(str.str)>()(str.str);
  }
};

/// @brief std::hash for JSSymbol. Injected into std namespace.
template <>
struct std::hash<njs::JSSymbol>
{
  std::size_t operator () (const njs::JSSymbol& symbol) const {
    return njs::hash_val(symbol.name, symbol.seq);
  }
};

#endif // NJS_RCOBJECT_H