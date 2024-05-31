#ifndef NJS_SYMBOL_TABLE_H
#define NJS_SYMBOL_TABLE_H

#include <cstdint>
#include <string>
#include "njs/common/enums.h"

namespace njs {

using u32 = uint32_t;
using std::u16string_view;

struct SymbolRecord {

  SymbolRecord() = default;

  SymbolRecord(VarKind kind, u16string_view name) : SymbolRecord(kind, name, 0, false) {}

  SymbolRecord(VarKind kind, u16string_view name, u32 index, bool is_special)
      : var_kind(kind), name(name), index(index), is_special(is_special) {}

  bool is_let_or_const() const {
    return var_kind == VarKind::DECL_LET || var_kind == VarKind::DECL_CONST;
  }

  VarKind var_kind;
  u16string_view name;
  u32 index;
  bool is_captured {false};
  bool is_special {false};
  bool referenced {false};
};

} // namespace njs

#endif // NJS_SYMBOL_TABLE_H