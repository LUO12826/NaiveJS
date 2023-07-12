#ifndef NJS_RCOBJECT_H
#define NJS_RCOBJECT_H

#include <cstdint>
#include <string>
#include "njs/utils/helper.h"

namespace njs {

using u32 = uint32_t;

class RCObject {
 public:
  RCObject() = default;
  virtual ~RCObject() = default;

  RCObject(const RCObject& obj) = delete;
  RCObject(RCObject&& obj) = delete;

  void retain();
  void release();
  void delete_temp_object();

 private:
  u32 ref_count {0};
};

/// @brief PrimitiveString: string that is not wrapped as objects in JavaScript
struct PrimitiveString: public RCObject {

  PrimitiveString() = default;

  explicit PrimitiveString(const std::u16string& str);

  bool operator == (const PrimitiveString& other) const;

  std::u16string str;
};

/// @brief https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol
struct JSSymbol: public RCObject {

  static size_t global_count;

  explicit JSSymbol(std::u16string name);

  bool operator == (const JSSymbol& other) const;

  std::string to_string();

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