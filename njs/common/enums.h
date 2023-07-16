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
  CLOSURE
};

ScopeType scope_type_from_int(int val);

int scope_type_int(ScopeType type);

VarKind get_var_kind_from_str(std::u16string_view str);

bool var_kind_allow_redeclare(VarKind kind);

std::string get_var_kind_str(VarKind kind);

} // namespace njs

#endif // NJS_ENUMS_H