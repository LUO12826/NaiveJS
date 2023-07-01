#ifndef NJS_SCOPE_H
#define NJS_SCOPE_H

#include "SymbolTable.h"
#include "njs/include/robin_hood.h"

namespace njs {

using llvm::SmallVector;
using std::u16string;
using std::u16string_view;
using robin_hood::unordered_set;
using robin_hood::unordered_map;

class Scope {
 public:
  enum Type {
    GLOBAL_SCOPE,
    FUNC_SCOPE,
    BLOCK_SCOPE
  };

  Scope(): type(GLOBAL_SCOPE) {}
  Scope(Type type): type(type) {}
  Scope(Type type, Scope *parent): type(type), parent_scope(parent) {}

  std::string get_scope_type_name() {
    switch (type) {
      case GLOBAL_SCOPE: return "GLOBAL_SCOPE";
      case FUNC_SCOPE: return "FUNC_SCOPE";
      case BLOCK_SCOPE: return "BLOCK_SCOPE";
      default: assert(false);
    }
  }

  bool define_func_parameter(u16string_view name, bool strict = false) {
    assert(type == FUNC_SCOPE);

    if (symbol_table.count(name) > 0) {
      return strict ? false : true;
    }

    symbol_table.emplace(name, SymbolRecord(VarKind::DECL_FUNC_PARAM, name, param_count));
    param_count += 1;
    return true;
  }

  bool define_symbol(VarKind var_kind, u16string_view name) {

    if (var_kind == VarKind::DECL_VAR || var_kind == VarKind::DECL_FUNCTION) {
      if (type != GLOBAL_SCOPE && type != FUNC_SCOPE) {
        assert(parent_scope);
        return parent_scope->define_symbol(var_kind, name);
      }
    }

    if (symbol_table.count(name) > 0) {
      bool can_redeclare = var_kind_allow_redeclare(var_kind)
                            && var_kind_allow_redeclare(symbol_table.at(name).var_kind);
      return can_redeclare;
    }

    symbol_table.emplace(name, SymbolRecord(var_kind, name, local_var_count));
    local_var_count += 1;

    return true;
  }

  void set_parent(Scope *parent) {
    this->parent_scope = parent;
  }

  unordered_map<u16string_view, SymbolRecord>& get_symbol_table() {
    return symbol_table;
  }

  uint32_t param_count {0};
  uint32_t local_var_count {0};
  
 private:
  Type type;
  
  unordered_map<u16string_view, SymbolRecord> symbol_table;
  // SmallVector<SymbolRecord, 10> symbol_table;
  Scope *parent_scope {nullptr};
};

} // namespace njs

#endif // NJS_SCOPE_H