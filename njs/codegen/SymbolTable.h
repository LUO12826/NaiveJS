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

  SymbolRecord(VarKind kind, u16str_view name) : SymbolRecord(kind, name, 0) {}

  SymbolRecord(VarKind kind, u16str_view name, u32 index)
      : var_kind(kind), name(name), index(index) {
    if (kind == VarKind::DECL_VAR || kind == VarKind::DECL_FUNCTION) { is_valid = true; }
  }

  u32 offset_idx(int offset = 2) { return index + offset; }

  VarKind var_kind;
  u16str_view name;
  u32 index;
  bool is_valid {false};
  bool is_captured {false};
};

} // namespace njs

#endif // NJS_SYMBOL_TABLE_H