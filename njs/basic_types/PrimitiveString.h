#ifndef NJS_PRIMITIVE_STRING_H
#define NJS_PRIMITIVE_STRING_H

#include <string>
#include "njs/gc/GCObject.h"
#include "njs/common/conversion_helper.h"

namespace njs {

/// @brief PrimitiveString: string that is not wrapped as objects in JavaScript
struct PrimitiveString: public GCObject {

  PrimitiveString() = default;

  explicit PrimitiveString(const std::u16string& str);
  explicit PrimitiveString(std::u16string&& str);

  bool operator == (const PrimitiveString& other) const;
  bool operator != (const PrimitiveString& other) const;
  bool operator < (const PrimitiveString& other) const;
  bool operator > (const PrimitiveString& other) const;
  bool operator >= (const PrimitiveString& other) const;
  bool operator <= (const PrimitiveString& other) const;

  void gc_scan_children(njs::GCHeap &heap) override {}

  std::string description() override {
    return "PrimitiveString(" + to_u8string(str) + ")";
  }

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
