#ifndef NJS_SYMBOL_TABLE_H
#define NJS_SYMBOL_TABLE_H

#include "njs/common/enums.h"
#include "njs/include/SmallVector.h"
#include <cstdint>
#include <string>

namespace njs {

using llvm::SmallVector;
using u32 = uint32_t;
using u16str_view = std::u16string_view;

struct SymbolRecord {

  SymbolRecord() = default;

  SymbolRecord(VarKind kind, u16str_view name) : SymbolRecord(kind, name, 0, false) {}

  SymbolRecord(VarKind kind, u16str_view name, u32 index, bool is_builtin)
      : var_kind(kind), name(name), index(index), is_builtin(is_builtin) {}

  u32 offset_idx(int offset = 2) { return index + offset; }

  bool is_let_or_const() {
    return var_kind == VarKind::DECL_LET || var_kind == VarKind::DECL_CONST;
  }

  VarKind var_kind;
  u16str_view name;
  u32 index;
  bool is_captured {false};
  bool is_builtin {false};
};

} // namespace njs

#endif // NJS_SYMBOL_TABLE_H