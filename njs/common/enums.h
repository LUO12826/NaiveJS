#ifndef NJS_ENUMS_H
#define NJS_ENUMS_H

#include <string>

namespace njs {

enum class VarKind {
  DECL_FUNCTION,
  DECL_VAR,
  DECL_LET,
  DECL_CONST,
  DECL_FUNC_PARAM,
};

VarKind get_var_kind_from_str(std::u16string_view str) {
  if (str == u"var") return VarKind::DECL_VAR;
  else if (str == u"let") return VarKind::DECL_LET;
  else if (str == u"const") return VarKind::DECL_CONST;
  else assert(false);
}

bool var_kind_allow_redeclare(VarKind kind) {
  return kind == VarKind::DECL_VAR
         || kind == VarKind::DECL_FUNCTION
         || kind == VarKind::DECL_FUNC_PARAM;
}

std::string get_var_kind_str(VarKind kind) {
  switch (kind) {
    case VarKind::DECL_FUNCTION: return "function";
    case VarKind::DECL_VAR: return "var";
    case VarKind::DECL_LET: return "let";
    case VarKind::DECL_CONST: return "const";
    case VarKind::DECL_FUNC_PARAM: return "(function parameter)";
    default: return "var";
  }
}

} // namespace njs

#endif // NJS_ENUMS_H