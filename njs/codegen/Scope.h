#ifndef NJS_SCOPE_H
#define NJS_SCOPE_H

#include "SymbolRecord.h"
#include "njs/common/enum_strings.h"
#include "njs/common/enums.h"
#include "njs/include/robin_hood.h"
#include "njs/vm/Instructions.h"
#include <memory>
#include <optional>

namespace njs {

using llvm::SmallVector;
using std::u16string;
using std::u16string_view;
using robin_hood::unordered_set;
using robin_hood::unordered_map;
using std::optional;
using std::unique_ptr;
using u32 = uint32_t;

class Function;
/*
 * Additional, on-demand scope-related information.
 */
struct ScopeContext {
  int64_t continue_pos {-1};
  bool can_break {false};
  std::vector<u32> break_list;
};

class Scope {
 public:

  struct SymbolResolveResult {

    u32 get_index() {
      if (storage_scope == ScopeType::GLOBAL || storage_scope == ScopeType::FUNC) {
        return index + 2;
      }
      return index;
    }

    bool stack_scope() {
      return storage_scope != ScopeType::CLOSURE && storage_scope != ScopeType::BLOCK;
    }

    bool not_found() { return original_symbol == nullptr; }

    static SymbolResolveResult none;

    SymbolRecord *original_symbol {nullptr};
    ScopeType storage_scope;
    ScopeType def_scope;
    u32 index;
  };

  Scope(): scope_type(ScopeType::GLOBAL) {}
  Scope(ScopeType type, Scope *outer): scope_type(type), outer_scope(outer) {
    // Blocks does not have a separate storage space, so it must inherit the
    // local variable storage index from the outer scope
    if (type == ScopeType::BLOCK) {
      assert(outer);
      next_var_index = outer->next_var_index;
      update_var_count(next_var_index);
    }

    if (type == ScopeType::FUNC || type == ScopeType::GLOBAL) {
      outer_func = this;
    }
    else {
      assert(outer);
      outer_func = outer->outer_func;
    }
  }

  ScopeType get_scope_type() { return scope_type; }

  BlockType get_block_type() { return block_type; }

  void set_block_type(BlockType block_type) {
    this->block_type = block_type;
  }

  std::string get_scope_type_name() {
    return scope_type_names[static_cast<int>(scope_type)];
  }

  /// @brief Only for function scope: define a function parameter.
  /// @return Succeeded or not.
  bool define_func_parameter(u16string_view name, bool strict = true) {
    assert(scope_type == ScopeType::FUNC);

    if (symbol_table.contains(name)) {
      // Strict mode doesn't allow duplicated parameter name.
      return !strict;
    }

    symbol_table.emplace(name, SymbolRecord(VarKind::DECL_FUNC_PARAM, name, param_count, false));
    param_count += 1;
    return true;
  }

  /// @brief Define a symbol in this scope.
  /// @return Succeeded or not.
  bool define_symbol(VarKind var_kind, u16string_view name, bool is_builtin = false) {
    assert(var_kind != VarKind::DECL_FUNC_PARAM);
    // var... or function...
    if (var_kind == VarKind::DECL_VAR || var_kind == VarKind::DECL_FUNCTION) {
      assert(outer_func);
      if (outer_func != this) {
        return outer_func->define_symbol(var_kind, name);
      }
    }

    if (symbol_table.contains(name)) {
      bool can_redeclare = var_kind_allow_redeclare(var_kind)
                            && var_kind_allow_redeclare(symbol_table.at(name).var_kind);
      return can_redeclare;
    }

    symbol_table.emplace(name, SymbolRecord(var_kind, name, next_var_index, is_builtin));
    next_var_index += 1;
    update_var_count(next_var_index);

    return true;
  }

  /// @brief Variables defined by `let` or `const` are legal to access only after they are
  /// initialized. Once they are initialized, call this method to mark them as valid.
  /// TODO: The implementation logic here is still different from what it is in JavaScript
  void mark_symbol_as_valid(u16string_view name) {
    auto find_res = symbol_table.find(name);
    if (find_res != symbol_table.end()) find_res->second.valid = true;
  }

  SymbolResolveResult resolve_symbol(u16string_view name) {
    return resolve_symbol_impl(name, 0, false);
  }

  void register_function(Function *func) {
    inner_func_init_code.emplace(func, SmallVector<Instruction, 5>());
  }

  void set_outer(Scope *outer) {
    this->outer_scope = outer;
    if (scope_type == ScopeType::BLOCK) {
      assert(outer);
      next_var_index = outer->next_var_index;
      update_var_count(next_var_index);
    }
    if (scope_type == ScopeType::FUNC || scope_type == ScopeType::GLOBAL) {
      outer_func = this;
    }
    else {
      assert(outer);
      outer_func = outer->outer_func;
    }
  }

  unordered_map<u16string_view, SymbolRecord>& get_symbol_table() {
    return symbol_table;
  }

  unordered_map<Function *, SmallVector<Instruction, 5>>& get_inner_func_init_code() {
    return inner_func_init_code;
  }

  SmallVector<SymbolResolveResult, 5>& get_capture_list() {
    return capture_list;
  }

  Scope *get_outer_func() {
    return outer_func;
  }

  ScopeContext& get_context() {
    if (!context) context = std::make_unique<ScopeContext>();
    return *context.get();
  }

  int64_t resolve_continue_pos() {
    if (context && context->continue_pos != -1) {
      return context->continue_pos;
    }
    else if (scope_type != ScopeType::FUNC && scope_type != ScopeType::GLOBAL && outer_scope) {
      return outer_scope->resolve_continue_pos();
    }
    else {
      return -1;
    }
  }

  std::vector<u32> *resolve_break_list() {
    if (context && context->can_break) {
      return &context->break_list;
    }
    else if (scope_type != ScopeType::FUNC && scope_type != ScopeType::GLOBAL && outer_scope) {
      return outer_scope->resolve_break_list();
    }
    else {
      return nullptr;
    }
  }

  u32 get_next_var_index() const {
    return next_var_index;
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
  
 private:
  SymbolResolveResult resolve_symbol_impl(u16string_view name, u32 depth, bool nonlocal) {
    if (symbol_table.contains(name)) {
      SymbolRecord& rec = symbol_table[name];

      if (nonlocal && scope_type != ScopeType::GLOBAL) rec.is_captured = true;
      // TODO: check this
      if (!rec.valid && !nonlocal) return SymbolResolveResult::none;

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
      // directly, as this is not of concern in both cases.
      if (!escape_local || scope_type != ScopeType::FUNC) {
        return outer_scope->resolve_symbol_impl(name, depth + 1, escape_local);
      }
      // This variable is captured from the outer function scope
      auto res = outer_scope->resolve_symbol_impl(name, depth + 1, escape_local);
      if (!res.original_symbol) return SymbolResolveResult::none;

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
    if (var_kind == VarKind::DECL_VAR) return this->scope_type;      // should be global or function
    if (var_kind == VarKind::DECL_FUNCTION) return this->scope_type; // should be global or function
    if (var_kind == VarKind::DECL_FUNC_PARAM) return ScopeType::FUNC_PARAM;

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

  unique_ptr<ScopeContext> context;

  u32 param_count {0};
  u32 next_var_index {0};
  u32 var_count {0};

  // for function scope
  SmallVector<SymbolResolveResult, 5> capture_list;
  unordered_map<Function *, SmallVector<Instruction, 5>> inner_func_init_code;
};

} // namespace njs

#endif // NJS_SCOPE_H