#ifndef NJS_CODEGEN_VISITOR_H
#define NJS_CODEGEN_VISITOR_H

#include <string>
#include <cstdint>
#include <iostream>
#include "Scope.h"
#include "njs/common/enums.h"
#include "njs/utils/helper.h"
#include "njs/include/SmallVector.h"
#include "njs/parser/ast.h"
#include "njs/include/robin_hood.h"
#include "njs/vm/Instructions.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/common/StringPool.h"

namespace njs {

using robin_hood::unordered_map;
using std::u16string;
using u32 = uint32_t;

struct CodegenError {
  std::string message;
  ASTNode* ast_node;
};

struct ScopeContext {
  explicit ScopeContext(ScopeContext *outer): outer(outer) {}

  ScopeContext *outer;
  SmallVector<Instruction, 10> temp_code_storage;
};

class CodegenVisitor {
friend class NjsVM;
 public:

  unordered_map<u16string, u32> global_props_map;

  void codegen(ProgramOrFunctionBody *prog) {
    visit_program_or_function_body(*prog);
    optimize();

    std::cout << "================ codegen result ================" << std::endl << std::endl;

    std::cout << ">>> instructions:" << std::endl;
    for (auto& inst : bytecode) {
      std::cout << inst.description() << std::endl;
    }
    std::cout << std::endl;

    str_list = str_pool.to_list();

    std::cout << ">>> string pool:" << std::endl;
    for (auto& str : str_list) {
      std::cout << to_utf8_string(str) << std::endl;
    }
    std::cout << std::endl;

    std::cout << ">>> number pool:" << std::endl;
    for (auto num : num_list) {
      std::cout << num << std::endl;
    }
    std::cout << std::endl;

    std::cout << ">>> function metadata:" << std::endl;
    for (auto& meta : func_meta) {
      std::cout << meta.description() << std::endl;
    }
    std::cout << std::endl;
    std::cout << "============== end codegen result ==============" << std::endl << std::endl;
  }

  void optimize() {
    size_t len = bytecode.size();

    if (len < 2) return;

    for (size_t i = 1; i < len; i++) {
      auto& inst = bytecode[i];
      auto& prev_inst = bytecode[i - 1];

      if (inst.op_type == InstType:: push && prev_inst.op_type == InstType::pop) {
        if (inst.operand.two.opr1 == prev_inst.operand.two.opr1
            && inst.operand.two.opr2 == prev_inst.operand.two.opr2) {
          inst.op_type = InstType::nop;
          prev_inst.op_type = InstType::store;
        }
      }
      if (inst.op_type == InstType:: push && prev_inst.op_type == InstType::pop_assign) {
        if (inst.operand.two.opr1 == prev_inst.operand.two.opr1
            && inst.operand.two.opr2 == prev_inst.operand.two.opr2) {
          inst.op_type = InstType::nop;
          prev_inst.op_type = InstType::store_assign;
        }
      }
    }

  }

  void visit(ASTNode *node) {
    switch (node->get_type()) {
      case ASTNode::AST_PROGRAM:
      case ASTNode::AST_FUNC_BODY:
        visit_program_or_function_body(*static_cast<ProgramOrFunctionBody *>(node));
        break;
      case ASTNode::AST_FUNC:
        visit_function(*static_cast<Function *>(node));
        break;
      case ASTNode::AST_EXPR_BINARY:
        visit_binary_expr(*static_cast<BinaryExpr *>(node));
        break;
      case ASTNode::AST_EXPR_ASSIGN:
        visit_assignment_expr(*static_cast<AssignmentExpr *>(node));
        break;
      case ASTNode::AST_EXPR_LHS:
        visit_left_hand_side_expr(*static_cast<LeftHandSideExpr *>(node), false);
        break;
      case ASTNode::AST_EXPR_ID:
        visit_identifier(*node);
        break;
      case ASTNode::AST_EXPR_ARGS:
        visit_func_arguments(*static_cast<Arguments *>(node));
        break;
      case ASTNode::AST_EXPR_NUMBER: visit_number_literal(*node);
        break;
      case ASTNode::AST_EXPR_STRING: visit_string_literal(*node);
        break;
      case ASTNode::AST_LIT_OBJ:
        visit_object_literal(*static_cast<ObjectLiteral *>(node));
        break;
      case ASTNode::AST_LIT_ARRAY:
        visit_array_literal(*static_cast<ArrayLiteral *>(node));
        break;
      case ASTNode::AST_STMT_VAR:
        visit_variable_statement(*static_cast<VarStatement *>(node));
        break;
      case ASTNode::AST_STMT_VAR_DECL:
        visit_variable_declaration(*static_cast<VarDecl *>(node));
        break;
      case ASTNode::AST_STMT_RETURN:
        visit_return_statement(*static_cast<ReturnStatement *>(node));
        break;
      default:
        assert(false);
    }
  }

  void visit_program_or_function_body(ProgramOrFunctionBody& program) {

    push_scope(std::move(program.scope));

    auto& jmp_inst = emit(InstType::jmp);

    // generate bytecode for functions first
    for (Function *func : program.func_decls) {
      visit(func);
    }
    // skip function bytecode
    jmp_inst.operand.two.opr1 = bytecode_pos();

    // Fill the function symbol initialization code to the very beginning
    // (because the function variable will be hoisted)

    for (auto& inst : current_context().temp_code_storage) {
      bytecode.push_back(inst);
    }
    current_context().temp_code_storage.clear();

    for (ASTNode *node : program.stmts) {
      visit(node);
    }

    if (program.type == ASTNode::AST_PROGRAM) {
      emit(InstType::halt);
    }

    // want to keep the global scope.
    if (program.type != ASTNode::AST_PROGRAM) {
      pop_scope();
    }
  }

  void visit_function(Function& func) {
    // currently only support function statement
    assert(func.is_stmt);

    // add the function name to the constant pool
    u32 name_cst_idx = add_const(func.get_name_str());

    // create metadata for function
    u32 func_idx = add_function_meta( JSFunctionMeta {
        .name_index = name_cst_idx,
        .code_address = bytecode_pos(),
    });

    // generate bytecode for function body
    visit(func.body);

    // let the VM make a function
    emit_temp(InstType::make_func, func_idx);

    // then put this function to where its name (as a variable) is located.
    auto symbol = current_scope().resolve_symbol(func.name.text);
    emit_temp(InstType::pop, scope_type_to_int(symbol.scope_type), symbol.symbol->index);
  }

  void visit_binary_expr(BinaryExpr& expr) {
    assert(expr.op.type == Token::ADD);
    if (expr.is_simple_expr()) {

      auto lhs_sym = current_scope().resolve_symbol(expr.lhs->get_source());
      auto rhs_sym = current_scope().resolve_symbol(expr.rhs->get_source());

      emit(InstType::fast_add,
           scope_type_to_int(lhs_sym.scope_type), lhs_sym.symbol->index,
           scope_type_to_int(rhs_sym.scope_type), rhs_sym.symbol->index);
    }
    else {
      assert(false);
    }
  }

  void visit_assignment_expr(AssignmentExpr& expr) {
//    assert();
    if (expr.is_simple_assign()) {
      auto lhs_sym = current_scope().resolve_symbol(expr.lhs->get_source());
      auto rhs_sym = current_scope().resolve_symbol(expr.rhs->get_source());

      if (lhs_sym.stack_scope() && rhs_sym.stack_scope()) {
        emit(InstType::fast_assign,
             scope_type_to_int(lhs_sym.scope_type), lhs_sym.symbol->index,
             scope_type_to_int(rhs_sym.scope_type), rhs_sym.symbol->index);
      }
      else {

      }
    }
    else if (expr.lhs_is_id()) {
      visit(expr.rhs);
      auto lhs_sym = current_scope().resolve_symbol(expr.lhs->get_source());
      if (lhs_sym.stack_scope()) {
        emit(InstType::pop_assign, scope_type_to_int(lhs_sym.scope_type), (int)lhs_sym.symbol->index);
      }
    }
    else {
      // check if left hand side is LeftHandSide Expression (or Parenthesized Expression with
      // LeftHandSide Expression in it.
      if (expr.lhs->type == ASTNode::AST_EXPR_LHS) {
        visit_left_hand_side_expr(*static_cast<LeftHandSideExpr*>(expr.lhs), true);
      }
      else if (expr.lhs->type == ASTNode::AST_EXPR_PAREN) {
        visit_left_hand_side_expr(*expr.lhs->as_paren_expr()->expr->as_lhs_expr(), true);
      }
      else assert(false);

      visit(expr.rhs);
      emit(InstType::prop_assign);
    }

  }

  void visit_object_literal(ObjectLiteral& literal) {
    emit(InstType::make_obj);

    for (auto& prop : literal.properties) {
      // push the key into the stack
      emit(InstType::push_str, (int)add_const(prop.key.text));

      // push the value into the stack
      visit(prop.value);
    }

    if (!literal.properties.empty()) {
      emit(InstType::add_props, (int)literal.properties.size());
    }
  }

  void visit_array_literal(ArrayLiteral& literal) {

  }

  void visit_left_hand_side_expr(LeftHandSideExpr& expr, bool create_ref) {
    visit(expr.base);
    auto& postfix_ord = expr.postfix_order;

    for (size_t i = 0; i < expr.postfix_order.size(); i++) {
      auto postfix_type = postfix_ord[i].first;
      u32 idx = postfix_ord[i].second;

      if (postfix_type == LeftHandSideExpr::CALL) {
        visit_func_arguments(*(expr.args_list[idx]));
        emit(InstType::call, expr.args_list[idx]->args.size());
      }
      else if (postfix_type == LeftHandSideExpr::PROP) {

        size_t prop_start = i;
        for (; postfix_ord[i].first == LeftHandSideExpr::PROP; i++) {
          idx = postfix_ord[i].second;
          int keypath_id = (int)add_const(expr.prop_list[idx].text);
          emit(InstType::push_atom, keypath_id);
        }

        emit(InstType::keypath_visit, int(i - prop_start), int(create_ref));

        i -= 1;
      }
      else if (postfix_type == LeftHandSideExpr::INDEX) {

      }
    }
  }

  void visit_identifier(ASTNode& id) {
    auto symbol = current_scope().resolve_symbol(id.get_source());
    emit(InstType::push, scope_type_to_int(symbol.scope_type), symbol.symbol->index);
  }

  void visit_func_arguments(Arguments& args) {
    for (auto node : args.args) {
      visit(node);
    }
  }

  // fixme: just a temporary method for number parse
  int64_t parse_number_literal(std::u16string_view str) {
    int64_t value = 0;

    if (str.size() >= 2 && str.substr(0, 2) == u"0x") {
        try {
          value = std::stoll(std::string(str.begin() + 2, str.end()), nullptr, 16);
        }
        catch (const std::exception& e) {
          std::cerr << "Failed to parse hex number: " << e.what() << std::endl;
        }
    }
    else {
        try {
          value = std::stoll(std::string(str.begin(), str.end()), nullptr, 10);
        }
        catch (const std::exception& e) {
          std::cerr << "Failed to parse decimal number: " << e.what() << std::endl;
        }
    }

    return value;
  }

  void visit_number_literal(ASTNode& node) {
    int64_t val = parse_number_literal(node.get_source());
    emit(Instruction::num_imm(val));
  }

  void visit_string_literal(ASTNode& node) {
    emit(InstType::push_str, (int)add_const(node.get_source()));
  }

  void visit_variable_statement(VarStatement& var_stmt) {
    for (VarDecl *decl : var_stmt.declarations) {
        visit_variable_declaration(*decl);
    }
  }

  void visit_variable_declaration(VarDecl& var_decl) {
    visit(var_decl.var_init);
  }

  void visit_return_statement(ReturnStatement& return_stmt) {
    visit(return_stmt.expr);
    emit(InstType::ret);
  }

 private:
  
  Scope& current_scope() { return scope_chain.back(); }

  ScopeContext& current_context() { return context_chain.back(); }

  ScopeContext& context_at(int index) {
    int idx = index >= 0 ? index : context_chain.size() + index;
    assert(idx >= 0 && idx <= context_chain.size());
    return context_chain[idx];
  }

  void push_scope(ScopeType scope_type) {
    Scope *outer = scope_chain.size() > 0 ? &scope_chain.back() : nullptr;
    scope_chain.emplace_back(scope_type, outer);

    context_chain.emplace_back(context_chain.size() > 0 ? &context_chain.back() : nullptr);
  }

  void push_scope(Scope&& scope) {
    Scope *outer = scope_chain.size() > 0 ? &scope_chain.back() : nullptr;
    scope.set_outer(outer);
    scope_chain.emplace_back(std::move(scope));

    context_chain.emplace_back(context_chain.size() > 0 ? &context_chain.back() : nullptr);
  }

  void pop_scope() {
    scope_chain.pop_back();
    context_chain.pop_back();
  }

  template <typename... Args>
  Instruction& emit(Args &&...args) {
    bytecode.emplace_back(std::forward<Args>(args)...);
    return bytecode.back();
  }

  Instruction& emit(Instruction inst) {
    bytecode.push_back(inst);
    return bytecode.back();
  }

  template <typename... Args>
  Instruction& emit_temp(Args &&...args) {
    context_at(-1).temp_code_storage.emplace_back(std::forward<Args>(args)...);
    return context_at(-1).temp_code_storage.back();
  }

  void report_error(CodegenError err) {
    error.push_back(std::move(err));
  }

  u32 add_const(u16string str) {
    auto idx = str_pool.add_string(std::move(str));
    return idx;
  }

  u32 add_const(u16string_view str_view) {
    auto idx = str_pool.add_string(str_view);
    return idx;
  }

  u32 add_const(double num) {
    auto idx = num_list.size();
    num_list.push_back(num);
    return idx;
  }

  u32 add_function_meta(JSFunctionMeta meta) {
    auto idx = func_meta.size();
    func_meta.push_back(meta);
    return idx;
  }

  u32 bytecode_pos() {
    return bytecode.size();
  }

  SmallVector<CodegenError, 10> error;

  SmallVector<Scope, 10> scope_chain;

  SmallVector<ScopeContext, 10> context_chain;

  SmallVector<Instruction, 10> bytecode;

  // for constant
  StringPool str_pool;
  SmallVector<u16string , 10> str_list;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  // for atom
  SmallVector<u16string, 10> atom_pool;

};

} // namespace njs

#endif // NJS_CODEGEN_VISITOR_H