#ifndef NJS_PRIMITIVE_STRING_H
#define NJS_PRIMITIVE_STRING_H

#include "RCObject.h"
#include "njs/utils/lexing_helper.h"
#include <string>

namespace njs {

/// @brief PrimitiveString: string that is not wrapped as objects in JavaScript
struct PrimitiveString: public RCObject {

  PrimitiveString() = default;

  explicit PrimitiveString(const std::u16string& str);
  explicit PrimitiveString(std::u16string&& str);

  RCObject *copy() override {
    return new PrimitiveString(this->str);
  }

  bool operator == (const PrimitiveString& other) const;
  bool operator != (const PrimitiveString& other) const;
  bool operator < (const PrimitiveString& other) const;
  bool operator > (const PrimitiveString& other) const;
  bool operator >= (const PrimitiveString& other) const;
  bool operator <= (const PrimitiveString& other) const;

  int64_t convert_to_index() const;
  size_t length() const {
    return str.length();
  }

  std::u16string str;
};

inline PrimitiveString::PrimitiveString(const std::u16string& str): str(str) {}
inline PrimitiveString::PrimitiveString(std::u16string&& str): str(std::move(str)) {}

inline bool PrimitiveString::operator == (const PrimitiveString& other) const {
  return str == other.str;
}

inline bool PrimitiveString::operator != (const PrimitiveString& other) const {
  return str != other.str;
}

inline bool PrimitiveString::operator < (const PrimitiveString& other) const {
  return str < other.str;
}

inline bool PrimitiveString::operator > (const PrimitiveString& other) const {
  return str > other.str;
}

inline bool PrimitiveString::operator >= (const PrimitiveString& other) const {
  return str >= other.str;
}

inline bool PrimitiveString::operator <= (const PrimitiveString& other) const {
  return str <= other.str;
}

inline int64_t PrimitiveString::convert_to_index() const {
  return scan_index_literal(this->str);
}

} // namespace njs

/// @brief std::hash for PrimitiveString. Injected into std namespace.
template <>
struct std::hash<njs::PrimitiveString>
{
  std::size_t operator () (const njs::PrimitiveString& str) const {
    return std::hash<decltype(str.str)>()(str.str);
  }
};

#endif //NJS_PRIMITIVE_STRING_H
