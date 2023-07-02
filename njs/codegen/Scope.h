#ifndef NJS_SCOPE_H
#define NJS_SCOPE_H

#include <optional>
#include "njs/common/enums.h"
#include "SymbolTable.h"
#include "njs/include/robin_hood.h"

namespace njs {

using llvm::SmallVector;
using std::u16string;
using std::u16string_view;
using robin_hood::unordered_set;
using robin_hood::unordered_map;
using std::optional;
using u32 = uint32_t;


class Scope {
 public:

  struct SymbolResolveResult {
    SymbolRecord* symbol {nullptr};
    u32 depth;
    ScopeType scope_type;
  };

  Scope(): scope_type(ScopeType::GLOBAL) {}
  Scope(ScopeType type): scope_type(type) {}
  Scope(ScopeType type, Scope *parent): scope_type(type), outer_scope(parent) {}

  std::string get_scope_type_name() {
    switch (scope_type) {
      case ScopeType::GLOBAL: return "GLOBAL_SCOPE";
      case ScopeType::FUNC: return "FUNC_SCOPE";
      case ScopeType::FUNC_PARAM: return "FUNC_PARAM_SCOPE";
      case ScopeType::BLOCK: return "BLOCK_SCOPE";
      case ScopeType::CLOSURE: return "CLOSURE_SCOPE";
      default: assert(false);
    }
  }

  bool define_func_parameter(u16string_view name, bool strict = false) {
    assert(scope_type == ScopeType::FUNC);

    if (symbol_table.count(name) > 0) {
      return strict ? false : true;
    }

    symbol_table.emplace(name, SymbolRecord(VarKind::DECL_FUNC_PARAM, name, param_count));
    param_count += 1;
    return true;
  }

  bool define_symbol(VarKind var_kind, u16string_view name) {

    if (var_kind == VarKind::DECL_VAR || var_kind == VarKind::DECL_FUNCTION) {
      if (scope_type != ScopeType::GLOBAL && scope_type != ScopeType::FUNC) {
        assert(outer_scope);
        return outer_scope->define_symbol(var_kind, name);
      }
    }

    if (symbol_table.contains(name)) {
      bool can_redeclare = var_kind_allow_redeclare(var_kind)
                            && var_kind_allow_redeclare(symbol_table.at(name).var_kind);
      return can_redeclare;
    }

    symbol_table.emplace(name, SymbolRecord(var_kind, name, local_var_count));
    local_var_count += 1;

    return true;
  }

  SymbolResolveResult resolve_symbol(u16string_view name) {
    return resolve_symbol_impl(name, 0);
  }

  void set_outer(Scope *parent) {
    this->outer_scope = parent;
  }

  unordered_map<u16string_view, SymbolRecord>& get_symbol_table() {
    return symbol_table;
  }

  u32 param_count {0};
  u32 local_var_count {0};
  
 private:
  SymbolResolveResult resolve_symbol_impl(u16string_view name, u32 depth) {
    if (symbol_table.contains(name)) {

      SymbolRecord& rec = symbol_table[name];
      return SymbolResolveResult{
        .symbol = &rec,
        .depth = depth,
        // fixme
        .scope_type = rec.var_kind == VarKind::DECL_FUNC_PARAM ?
                                      ScopeType::FUNC_PARAM : scope_type
      };
    }

    if (outer_scope) return outer_scope->resolve_symbol_impl(name, depth + 1);

    return SymbolResolveResult();
  }

  ScopeType scope_type;
  
  unordered_map<u16string_view, SymbolRecord> symbol_table;
  Scope *outer_scope {nullptr};
};

} // namespace njs

#endif // NJS_SCOPE_H