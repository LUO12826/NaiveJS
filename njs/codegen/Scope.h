#ifndef NJS_SCOPE_H
#define NJS_SCOPE_H

#include "SymbolTable.h"

namespace njs {

using llvm::SmallVector;
using std::u16string;

class Scope {
 public:
  enum Type {
    GLOBAL_SCOPE,
    FUNC_SCOPE,
    BLOCK_SCOPE
  };

  Scope(Type type): type(type) {}

  void define_symbol(VarKind var_kind, std::u16string_view name) {
    symbol_table.emplace_back(var_kind, name, symbol_table.size());
  }

 private:
  Type type;
  SmallVector<u16string, 10> parameters;
  SmallVector<SymbolTableEntry, 10> symbol_table;
};

} // namespace njs

#endif // NJS_SCOPE_H