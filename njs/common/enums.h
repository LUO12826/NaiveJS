#ifndef NJS_ENUMS_H
#define NJS_ENUMS_H

#include <string>

namespace njs {

// has corresponding string representation, note to modify when adding
enum class VarKind {
  DECL_FUNCTION,
  DECL_VAR,
  DECL_LET,
  DECL_CONST,
  DECL_FUNC_PARAM,
};

// has corresponding string representation, note to modify when adding
enum class ScopeType {
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

enum class BlockType {
  NOT_BLOCK,
  PLAIN,
  IF,
  FOR,
  WHILE,
  SWITCH,
  TRY,
  CATCH
};

inline ScopeType scope_type_from_int(int val) {
  return static_cast<ScopeType>(val);
}

inline int scope_type_int(ScopeType type) {
  return static_cast<int>(type);
}

inline VarKind get_var_kind_from_str(std::u16string_view str) {
  if (str == u"var") return VarKind::DECL_VAR;
  else if (str == u"let") return VarKind::DECL_LET;
  else if (str == u"const") return VarKind::DECL_CONST;
  else assert(false);
  __builtin_unreachable();
}

inline bool var_kind_allow_redeclare(VarKind kind) {
  return kind == VarKind::DECL_VAR
         || kind == VarKind::DECL_FUNCTION
         || kind == VarKind::DECL_FUNC_PARAM;
}

inline std::string get_var_kind_str(VarKind kind) {
  switch (kind) {
    case VarKind::DECL_FUNCTION: return "function";
    case VarKind::DECL_VAR: return "var";
    case VarKind::DECL_LET: return "let";
    case VarKind::DECL_CONST: return "const";
    case VarKind::DECL_FUNC_PARAM: return "(func parameter)";
    default: return "(error)";
  }
}

} // namespace njs

#endif // NJS_ENUMS_H