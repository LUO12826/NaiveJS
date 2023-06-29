#ifndef NJS_ENUMS_H
#define NJS_ENUMS_H

#include <string>

namespace njs {

enum class VarKind {
  DECL_FUNCTION,
  DECL_VAR,
  DECL_LET,
  DECL_CONST
};

std::string get_var_kind_str(VarKind kind) {
  switch (kind) {
    case VarKind::DECL_FUNCTION: return "function";
    case VarKind::DECL_VAR: return "var";
    case VarKind::DECL_LET: return "let";
    case VarKind::DECL_CONST: return "const";
    default: return "var";
  }
}

} // namespace njs

#endif // NJS_ENUMS_H