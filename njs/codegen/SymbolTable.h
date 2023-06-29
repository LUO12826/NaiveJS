#ifndef NJS_SYMBOL_TABLE_H
#define NJS_SYMBOL_TABLE_H

#include <string>
#include <cstdint>
#include "Scope.h"
namespace njs {

using u32 = uint32_t;

enum class SymbolType {
  VARIABLE,
  FUNCTION,
  ARROW_FUNCTION,
};

struct SymbolTableEntry {
  std::u16string name;
  SymbolType type;
  int captured_scope;
};

class SymbolTable {
 public:
};

} // namespace njs

#endif // NJS_SYMBOL_TABLE_H