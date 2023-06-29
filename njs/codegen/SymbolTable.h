#ifndef NJS_SYMBOL_TABLE_H
#define NJS_SYMBOL_TABLE_H

#include "Scope.h"
#include "SmallVector.h"
#include "njs/common/enums.h"
#include <cstdint>
#include <string>

namespace njs {

using llvm::SmallVector;
using u32 = uint32_t;
using u16str_view = std::u16string_view;

struct SymbolTableEntry {

  SymbolTableEntry(VarKind kind, u16str_view name) : var_kind(kind), name(name) {}
  SymbolTableEntry(VarKind kind, u16str_view name, int index)
      : var_kind(kind), name(name), scope_index(index) {}

  VarKind var_kind;
  u16str_view name;
  int scope_index;
};

class SymbolTable {
 public:
 private:
  SmallVector<SymbolTableEntry, 10> table;
};

} // namespace njs

#endif // NJS_SYMBOL_TABLE_H