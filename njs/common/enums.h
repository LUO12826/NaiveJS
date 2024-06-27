#ifndef NJS_ENUMS_H
#define NJS_ENUMS_H

#include <string>
#include <cassert>
#include <cstdint>
#include "njs/common/common_types.h"

namespace njs {

using std::u16string_view;
using std::u16string;

// has corresponding string representation, note to modify when adding
enum class VarKind: uint8_t {
  FUNCTION,
  VAR,
  LET,
  CONST,
  FUNC_PARAM,
};

// has corresponding string representation, note to modify when adding
enum class ScopeType: uint8_t {
  GLOBAL = 0,
  FUNC,
  // `FUNC_PARAM` scope does not really exist. I made this enum value because, in my VM implementation,
  // function parameters and function local variables are addressed differently, and a mark must be
  // used to let the VM know to access the arguments within the argument store.
  FUNC_PARAM,
  BLOCK,
  // `CLOSURE` scope does not really exist.
  CLOSURE
};

enum class BlockType: uint8_t {
  NOT_BLOCK,
  PLAIN,
  IF_THEN,
  IF_ELSE,
  FOR,
  FOR_IN,
  WHILE,
  DO_WHILE,
  SWITCH,
  TRY,
  TRY_FINALLY,
  CATCH,
  CATCH_FINALLY,
  FINALLY_NORM,
  FINALLY_ECPT,
};

inline ScopeType scope_type_from_int(int val) {
  return static_cast<ScopeType>(val);
}

inline int scope_type_int(ScopeType type) {
  return static_cast<int>(type);
}

inline VarKind get_var_kind_from_str(u16string_view str) {
  if (str == u"var") return VarKind::VAR;
  else if (str == u"let") return VarKind::LET;
  else if (str == u"const") return VarKind::CONST;
  else assert(false);
  __builtin_unreachable();
}

inline bool var_kind_allow_redeclare(VarKind kind) {
  return kind == VarKind::VAR || kind == VarKind::FUNCTION || kind == VarKind::FUNC_PARAM;
}

inline std::string get_var_kind_str(VarKind kind) {
  switch (kind) {
    case VarKind::FUNCTION: return "function";
    case VarKind::VAR: return "var";
    case VarKind::LET: return "let";
    case VarKind::CONST: return "const";
    case VarKind::FUNC_PARAM: return "(func parameter)";
    default: return "(error)";
  }
}

} // namespace njs

#endif // NJS_ENUMS_H