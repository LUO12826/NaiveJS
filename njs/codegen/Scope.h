#ifndef NJS_SCOPE_H
#define NJS_SCOPE_H

#include <memory>
#include <optional>
#include <utility>
#include "SymbolRecord.h"
#include "njs/include/SmallVector.h"
#include "njs/common/enum_strings.h"
#include "njs/common/enums.h"
#include "njs/common/common_def.h"
#include "njs/include/robin_hood.h"
#include "njs/vm/Instruction.h"
#include "njs/codegen/CatchTableEntry.h"

namespace njs {

using llvm::SmallVector;
using std::u16string;
using std::u16string_view;
using robin_hood::unordered_set;
using robin_hood::unordered_map;
using std::optional;
using std::pair;
using std::unique_ptr;
using u32 = uint32_t;

class Function;
class ASTNode;

class Scope {
 public:

  struct SymbolResolveResult {

    u32 get_index() {
      if (storage_scope == ScopeType::GLOBAL || storage_scope == ScopeType::FUNC) {
        return index + frame_meta_size;
      }
      return index;
    }

    bool not_found() { return original_symbol == nullptr; }

    bool is_let_or_const() {
      assert(original_symbol);
      return original_symbol->is_let_or_const();
    }

    static SymbolResolveResult none;

    SymbolRecord *original_symbol {nullptr};
    ScopeType storage_scope;
    ScopeType def_scope;
    u32 index;
    bool is_this_function_name {false};
  };

  struct ControlRedirectResult {
    bool found {false};
    u32 stack_drop_cnt {0};
    Scope *target_scope {nullptr};
  };

  Scope(): scope_type(ScopeType::GLOBAL) {}
  Scope(ScopeType type, Scope *outer): scope_type(type) {
    set_outer(outer);
  }

  ScopeType get_type() { return scope_type; }

  BlockType get_block_type() { return block_type; }

  void set_block_type(BlockType type) {
    this->block_type = type;
  }

  std::string get_type_name() {
    return scope_type_names[static_cast<int>(scope_type)];
  }

  /// @brief Only for function scope: define a function parameter.
  /// @return Succeeded or not.
  bool define_func_parameter(u16string_view name, bool strict = true) {
    assert(scope_type == ScopeType::FUNC);

    auto find_res = symbol_table.find(name);
    if (find_res != symbol_table.end()) [[unlikely]] {
      if (find_res->second.is_special) {
        find_res->second = SymbolRecord(VarKind::FUNC_PARAM, name, param_count, false);
        param_count += 1;
        return true;
      }
      // Strict mode doesn't allow duplicated parameter name.
      return !strict;
    }

    symbol_table.emplace(name, SymbolRecord(VarKind::FUNC_PARAM, name, param_count, false));
    param_count += 1;
    return true;
  }

  /// @brief Define a symbol in this scope.
  /// @return Succeeded or not.
  bool define_symbol(VarKind var_kind, u16string_view name, bool is_special = false) {
    assert(var_kind != VarKind::FUNC_PARAM);
    // var... or function...
    if (var_kind == VarKind::VAR || var_kind == VarKind::FUNCTION) {
      assert(outer_func);
      if (outer_func != this) {
        return outer_func->define_symbol(var_kind, name, is_special);
      }
    }

    auto find_res = symbol_table.find(name);

    if (find_res != symbol_table.end()) [[unlikely]] {
      if (find_res->second.is_special) [[unlikely]] {
        find_res->second.is_special = is_special;
        goto redeclare;
      }

      if (var_kind_allow_redeclare(var_kind)
          && var_kind_allow_redeclare(find_res->second.var_kind)) {
      redeclare:
        find_res->second.var_kind = var_kind;
        return true;
      } else {
        return false;
      }
    }

    symbol_table.emplace(name, SymbolRecord(var_kind, name, var_idx_next, is_special));
    var_idx_next += 1;
    update_var_count(var_idx_next);

    return true;
  }

  SymbolResolveResult resolve_symbol(u16string_view name) {
    return resolve_symbol_impl(name, 0, false);
  }

  SymbolRecord* get_symbol_direct(u16string_view name) {
    if (symbol_table.contains(name)) {
      return &symbol_table[name];
    } else {
      return nullptr;
    }
  }

  void register_function(Function *func) {
    inner_func_order.push_back(func);
    inner_func_init_code.emplace(func, SmallVector<Instruction, 10>());
  }

  unordered_map<u16string_view, SymbolRecord>& get_symbol_table() {
    return symbol_table;
  }

  Scope *get_outer_func() {
    return outer_func;
  }

  void set_outer(Scope *outer) {
    this->outer_scope = outer;
    if (scope_type == ScopeType::BLOCK) {
      assert(outer);
      // Blocks does not have a separate storage space, so it must inherit the
      // local variable storage index from the outer scope
      var_idx_start = outer->var_idx_next;
      var_idx_next = outer->var_idx_next;
      update_var_count(var_idx_next);

      curr_stack_size = outer->curr_stack_size;
      max_stack_size = outer->max_stack_size;

      outer_func = outer->outer_func;
      is_continue_target = outer->can_continue;
      is_break_target = outer->can_break;
    }
    else if (scope_type == ScopeType::FUNC || scope_type == ScopeType::GLOBAL) {
      outer_func = this;
    }
    else {
      assert(false);
    }
  }

  Scope *get_outer() {
    return outer_scope;
  }

  SmallVector<u32, 3> *resolve_throw_list() {
    if (has_try || scope_type == ScopeType::FUNC || scope_type == ScopeType::GLOBAL) {
      return &throw_list;
    } else {
      assert(outer_scope);
      return outer_scope->resolve_throw_list();
    }
  }

  u32 get_var_next_index() const {
    return var_idx_next;
  }

  u32 get_var_start_index() const {
    return var_idx_start;
  }

  pair<u32, u32> get_var_index_range(int offset = 0) const {
    return {var_idx_start + offset, var_idx_next + offset};
  }

  u32 get_max_stack_size() {
    return max_stack_size;
  }

  u32 get_var_count() const {
    return var_count;
  }

  u32 get_param_count() const {
    return param_count;
  }

  void update_var_count(u32 count) {
    var_count = std::max(count, var_count);
  }

  void update_stack_usage(int diff) {
    curr_stack_size += diff;
    assert(curr_stack_size >= 0);
    max_stack_size = std::max(max_stack_size, curr_stack_size);
  }

  void update_max_stack_size(int new_max) {
    max_stack_size = std::max(max_stack_size, new_max);
  }
  
 private:
  SymbolResolveResult resolve_symbol_impl(u16string_view name, u32 depth, bool nonlocal) {
    if (symbol_table.contains(name)) {
      SymbolRecord& rec = symbol_table[name];
      rec.referenced = true;

      if (nonlocal && scope_type != ScopeType::GLOBAL) rec.is_captured = true;

      return SymbolResolveResult{
          .original_symbol = &rec,
          .storage_scope = get_storage_scope(rec.var_kind),
          .def_scope = this->scope_type,
          .index = rec.index,
      };
    }

    if (scope_type == ScopeType::FUNC) {
      for (size_t i = 0; i < capture_list.size(); i++) {
        if (name != capture_list[i].original_symbol->name) continue;

        return SymbolResolveResult{
            .original_symbol = capture_list[i].original_symbol,
            .storage_scope = ScopeType::CLOSURE,
            .def_scope = ScopeType::CLOSURE,
            .index = u32(i),
        };
      }
    }

    if (outer_scope) {
      // Here we want to determine whether a closure variable capture has occurred,
      // i.e. whether the retrieval of a variable is outside the scope of this function.
      bool escape_local = nonlocal || scope_type == ScopeType::FUNC;

      // If no capture occurs or if this scope is a block scope, then the result is returned
      // directly. no additional processing is needed.
      if (!escape_local || scope_type != ScopeType::FUNC) {
        return outer_scope->resolve_symbol_impl(name, depth + 1, escape_local);
      }
      // This variable is captured from the outer function scope
      auto res = outer_scope->resolve_symbol_impl(name, depth + 1, escape_local);
      if (res.not_found()) return SymbolResolveResult::none;

      // global variables are always available. We don't need to capture them.
      if (res.storage_scope == ScopeType::GLOBAL && res.def_scope == ScopeType::GLOBAL) {
        return res;
      }

      capture_list.push_back(res);
      return SymbolResolveResult{
          .original_symbol = res.original_symbol,
          .storage_scope = ScopeType::CLOSURE,
          .def_scope = res.def_scope,
          .index = (u32)capture_list.size() - 1
      };
    }

    return SymbolResolveResult::none;
  }

  ScopeType get_storage_scope(VarKind var_kind) {
    if (var_kind == VarKind::VAR) return this->scope_type;      // should be global or function
    if (var_kind == VarKind::FUNCTION) return this->scope_type; // should be global or function
    if (var_kind == VarKind::FUNC_PARAM) return ScopeType::FUNC_PARAM;

    // let or const
    // Blocks does not have a separate storage space, so `let` and `const` are stored in
    // the nearest function or global scope.
    return outer_func->scope_type;
  }

  ScopeType scope_type;
  BlockType block_type {BlockType::NOT_BLOCK};
  
  unordered_map<u16string_view, SymbolRecord> symbol_table;
  Scope *outer_scope {nullptr};
  Scope *outer_func {nullptr}; // this can be global scope

  u32 param_count {0};
  u32 var_idx_start {0};
  u32 var_idx_next {0};
  u32 var_count {0};

  int max_stack_size {0};
  int curr_stack_size {0};

 public:
  int64_t continue_pos {-1};
  // If this is set to true, that means this scope is the scope that directly contains a
  // `while`, `for` or `do-while` and we are currently dealing with the loop statement.
  bool can_break {false};
  bool can_continue {false};
  // If this scope is the one that `continue`s inside will come to, set it to true.
  bool is_continue_target {false};
  bool is_break_target {false};
  bool has_try {false};
  bool is_strict {false};
  ASTNode *associated_node {nullptr};
  SmallVector<u32, 3> break_list;
  SmallVector<u32, 3> continue_list;
  SmallVector<u32, 3> throw_list;
  SmallVector<CatchTableEntry, 3> catch_table;
  // for try-catch-finally
  SmallVector<u32, 3> call_procedure_list;
  // only function or global has this
  SmallVector<u16string_view, 3> label_stack;

  // for function scope
  SmallVector<SymbolResolveResult, 5> capture_list;
  unordered_map<Function*, SmallVector<Instruction, 10>> inner_func_init_code;
  SmallVector<Function*, 3> inner_func_order;
};

inline Scope::SymbolResolveResult  Scope::SymbolResolveResult::none = SymbolResolveResult();

} // namespace njs

#endif // NJS_SCOPE_H