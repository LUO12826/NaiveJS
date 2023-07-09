#ifndef NJS_SCOPE_H
#define NJS_SCOPE_H

#include <optional>
#include "njs/common/enums.h"
#include "njs/common/enum_strings.h"
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

    static SymbolResolveResult none;

    SymbolRecord* symbol {nullptr};
    ScopeType scope_type;
    u32 closure_idx;

    bool stack_scope() {
      return scope_type != ScopeType::CLOSURE && scope_type != ScopeType::BLOCK;
    }

    bool not_found() { return symbol == nullptr; }

    u32 get_index() {
      if (scope_type == ScopeType::CLOSURE) return closure_idx;
      return symbol->index;
    }
  };

  Scope(): scope_type(ScopeType::GLOBAL) {}
  Scope(ScopeType type, Scope *outer): scope_type(type), outer_scope(outer) {
    // Blocks does not have a separate storage space, so it must inherit the
    // local variable storage index from the upper scope
    if (type == ScopeType::BLOCK) {
      assert(outer);
      local_var_count = outer->local_var_count;
    }

    if (type == ScopeType::FUNC || type == ScopeType::GLOBAL) {
      outer_global_or_func = this;
    }
    else {
      assert(outer);
      outer_global_or_func = outer->outer_global_or_func;
    }
  }

  std::string get_scope_type_name() {
    return scope_type_names[static_cast<int>(scope_type)];
  }

  bool define_func_parameter(u16string_view name, bool strict = false) {
    assert(scope_type == ScopeType::FUNC);

    if (symbol_table.contains(name)) {
      return strict ? false : true;
    }

    symbol_table.emplace(name, SymbolRecord(VarKind::DECL_FUNC_PARAM, name, param_count));
    param_count += 1;
    return true;
  }

  bool define_symbol(VarKind var_kind, u16string_view name) {
    assert(var_kind != VarKind::DECL_FUNC_PARAM);
    // var... or function...
    if (var_kind == VarKind::DECL_VAR || var_kind == VarKind::DECL_FUNCTION) {
      if (scope_type != ScopeType::GLOBAL && scope_type != ScopeType::FUNC) {
        assert(outer_scope);
        return outer_scope->define_symbol(var_kind, name);
      }
    }

    // let... or const...
    if (symbol_table.contains(name)) {
      bool can_redeclare = var_kind_allow_redeclare(var_kind)
                            && var_kind_allow_redeclare(symbol_table.at(name).var_kind);
      return can_redeclare;
    }

    symbol_table.emplace(name, SymbolRecord(var_kind, name, local_var_count));
    local_var_count += 1;

    return true;
  }

  void mark_symbol_as_valid(u16string_view name) {
    auto find_res = symbol_table.find(name);
    if (find_res != symbol_table.end()) find_res->second.valid = true;
  }

  SymbolResolveResult resolve_symbol(u16string_view name) {
    return resolve_symbol_impl(name, 0, false);
  }

  void set_outer(Scope *parent) {
    this->outer_scope = parent;
  }

  unordered_map<u16string_view, SymbolRecord>& get_symbol_table() {
    return symbol_table;
  }

  SmallVector<SymbolResolveResult, 10>& get_capture_list() {
    return capture_list;
  }

  u32 param_count {0};
  u32 local_var_count {0};
  
 private:
  SymbolResolveResult resolve_symbol_impl(u16string_view name, u32 depth, bool nonlocal) {
    if (symbol_table.contains(name)) {
      SymbolRecord& rec = symbol_table[name];

      if (nonlocal && scope_type != ScopeType::GLOBAL) rec.is_captured = true;
      if (!rec.valid && !rec.is_captured) return SymbolResolveResult::none;

      return SymbolResolveResult{
        .symbol = &rec,
        .scope_type = get_storage_scope(rec.var_kind)
      };
    }

    if (outer_scope) {
      // Here we want to determine whether a closure variable capture has occurred,
      // i.e. whether the retrieval of a variable is outside the scope of this function.
      bool escape_local = nonlocal || scope_type == ScopeType::FUNC;

      // If no capture occurs or if this scope is a block scope, then the result is returned
      // directly, as this is not of concern in both cases.
      if (!escape_local || scope_type != ScopeType::FUNC) {
        return outer_scope->resolve_symbol_impl(name, depth + 1, escape_local);
      }
      // This variable is captured from the outer function scope
      auto res = outer_scope->resolve_symbol_impl(name, depth + 1, escape_local);
      if (!res.symbol) return SymbolResolveResult::none;

      // global variables are always available. We don't need to capture them.
      if (res.scope_type == ScopeType::GLOBAL) return res;

      capture_list.push_back(res);
      return SymbolResolveResult{
          .symbol = res.symbol,
          .scope_type = ScopeType::CLOSURE,
          .closure_idx = (u32)capture_list.size() - 1
      };
    }

    return SymbolResolveResult::none;
  }

  ScopeType get_storage_scope(VarKind var_kind) {
    if (var_kind == VarKind::DECL_VAR) return this->scope_type;      // should be global or function
    if (var_kind == VarKind::DECL_FUNCTION) return this->scope_type; // should be global or function
    if (var_kind == VarKind::DECL_FUNC_PARAM) return ScopeType::FUNC_PARAM;

    // let or const
    // Blocks does not have a separate storage space, so `let` and `const` are stored in
    // the nearest function or global scope.
    return outer_global_or_func->scope_type;
  }

  ScopeType scope_type;
  
  unordered_map<u16string_view, SymbolRecord> symbol_table;
  SmallVector<SymbolResolveResult, 10> capture_list;
  Scope *outer_scope {nullptr};
  Scope *outer_global_or_func {nullptr};
};

} // namespace njs

#endif // NJS_SCOPE_H