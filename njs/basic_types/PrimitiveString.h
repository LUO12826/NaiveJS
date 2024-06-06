#ifndef NJS_PRIMITIVE_STRING_H
#define NJS_PRIMITIVE_STRING_H

#include <string>
#include "njs/gc/GCObject.h"
#include "njs/basic_types/String.h"
#include "njs/common/conversion_helper.h"

namespace njs {

using std::u16string;

/// @brief PrimitiveString: string that is not wrapped as objects in JavaScript
struct PrimitiveString: public GCObject {

  PrimitiveString() = default;

  explicit PrimitiveString(u16string_view str_view);
  explicit PrimitiveString(const u16string& str);
  explicit PrimitiveString(const String& str);
  explicit PrimitiveString(String&& str);

  bool operator == (const PrimitiveString& other) const;
  bool operator != (const PrimitiveString& other) const;
  bool operator < (const PrimitiveString& other) const;
  bool operator > (const PrimitiveString& other) const;
  bool operator >= (const PrimitiveString& other) const;
  bool operator <= (const PrimitiveString& other) const;

  void gc_scan_children(njs::GCHeap &heap) override {}

  std::string description() override {
    return "PrimitiveString(" + str.to_std_u8string() + ")";
  }

  size_t length() const {
    return str.size();
  }

  String str;
};

inline PrimitiveString::PrimitiveString(u16string_view str_view): str(str_view) {}
inline PrimitiveString::PrimitiveString(const u16string& str): str(str) {}
inline PrimitiveString::PrimitiveString(const String& str): str(str) {}
inline PrimitiveString::PrimitiveString(String&& str): str(std::move(str)) {}

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
