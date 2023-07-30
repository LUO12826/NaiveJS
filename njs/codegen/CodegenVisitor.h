#ifndef NJS_CODEGEN_VISITOR_H
#define NJS_CODEGEN_VISITOR_H

#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include "Scope.h"
#include "njs/common/enums.h"
#include "njs/utils/helper.h"
#include "njs/include/SmallVector.h"
#include "njs/parser/ast.h"
#include "njs/include/robin_hood.h"
#include "njs/vm/Instructions.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/common/StringPool.h"
#include "njs/utils/Timer.h"

namespace njs {

using robin_hood::unordered_map;
using std::u16string;
using std::unique_ptr;
using std::vector;
using u32 = uint32_t;

struct CodegenError {
  std::string message;
  ASTNode* ast_node;
};

class CodegenVisitor {
friend class NjsVM;
 public:

  unordered_map<u16string, u32> global_props_map;

  void codegen(ProgramOrFunctionBody *prog) {

    push_scope(std::move(prog->scope));

    // add functions to the global scope
    add_builtin_functions();
    visit_program_or_function_body(*prog);

    Timer timer("optimized");
    optimize();
    timer.end();

    std::cout << "================ codegen result ================" << std::endl << std::endl;

    std::cout << ">>> instructions:" << std::endl;
    int i = 0;
    for (auto& inst : bytecode) {
      std::cout << std::setw(6) << std::left << i << inst.description() << std::endl;
      i += 1;
    }
    std::cout << std::endl;

    std::vector<u16string>& str_list = str_pool.get_string_list();

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
    int removed_inst_cnt = 0;

    if (len < 2) return;

    vector<int> pos_moved(len);
    pos_moved[0] = 0;

    for (size_t i = 1; i < len; i++) {
      auto& inst = bytecode[i];
      auto& prev_inst = bytecode[i - 1];
      pos_moved[i] = -removed_inst_cnt;

//      if (inst.op_type == InstType:: push && prev_inst.op_type == InstType::pop) {
//        if (inst.operand.two.opr1 == prev_inst.operand.two.opr1
//            && inst.operand.two.opr2 == prev_inst.operand.two.opr2) {
//          removed_inst_cnt += 1;
//          inst.op_type = InstType::nop;
//          prev_inst.op_type = InstType::store;
//        }
//      }

      if (inst.op_type == InstType::jmp_true || inst.op_type == InstType::jmp) {
        if (inst.operand.two.opr1 == i + 1) {
          removed_inst_cnt += 1;
          inst.op_type = InstType::nop;
        }

        if (inst.op_type == InstType::jmp
            && prev_inst.op_type == InstType::jmp_true
            && prev_inst.operand.two.opr1 == i + 1) {
          
          prev_inst.op_type = InstType::jmp_false;
          prev_inst.operand.two.opr1 = inst.operand.two.opr1;
          inst.op_type = InstType::nop;
          removed_inst_cnt += 1;
        }
      }
    }

    size_t new_inst_ptr = 0;
    for (size_t i = 0; i < len; i++) {

      if (bytecode[i].op_type != InstType::nop) {
        auto& inst = bytecode[i];
        // Fix the jump target of the jmp instructions
        if (inst.op_type == InstType::jmp || inst.op_type == InstType::jmp_true
            || inst.op_type == InstType::jmp_false) {
          inst.operand.two.opr1 += pos_moved[inst.operand.two.opr1];
        }
        bytecode[new_inst_ptr] = inst;
        new_inst_ptr += 1;
      }
    }
    // Fix the jump target of the call instructions
    for (auto& meta : func_meta) {
      if (meta.is_native) continue;
      meta.code_address += pos_moved[meta.code_address];
    }

    bytecode.resize(new_inst_ptr);
  }

  void visit(ASTNode *node) {
    switch (node->type) {
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
      case ASTNode::AST_EXPR_UNARY:
        visit_unary_expr(*static_cast<UnaryExpr *>(node));
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
      case ASTNode::AST_EXPR_NUMBER: visit_number_literal(*static_cast<NumberLiteral *>(node));
        break;
      case ASTNode::AST_EXPR_STRING: visit_string_literal(*node);
        break;
      case ASTNode::AST_EXPR_OBJ:
        visit_object_literal(*static_cast<ObjectLiteral *>(node));
        break;
      case ASTNode::AST_EXPR_ARRAY:
        visit_array_literal(*static_cast<ArrayLiteral *>(node));
        break;
      case ASTNode::AST_EXPR_NULL:
        emit(InstType::push_null);
        break;
      case ASTNode::AST_EXPR_THIS:
        emit(InstType::push_this);
        break;
      case ASTNode::AST_EXPR_PAREN:
        visit(static_cast<ParenthesisExpr *>(node)->expr);
        break;
      case ASTNode::AST_EXPR_BOOL:
        if (node->get_source() == u"true") emit(InstType::push_bool, u32(1));
        else emit(InstType::push_bool, u32(0));
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
      case ASTNode::AST_STMT_IF:
        visit_if_statement(*static_cast<IfStatement *>(node));
        break;
      case ASTNode::AST_STMT_WHILE:
        visit_while_statement(*static_cast<WhileStatement *>(node));
        break;
      case ASTNode::AST_STMT_CONTINUE:
      case ASTNode::AST_STMT_BREAK:
        visit_continue_break_statement(*static_cast<ContinueOrBreak *>(node));
        break;
      case ASTNode::AST_STMT_BLOCK:
        visit_block_statement(*static_cast<Block *>(node));
        break;
      default:
        std::cout << node->description() << " not supported yet" << std::endl;
        assert(false);
    }
  }

  void visit_program_or_function_body(ProgramOrFunctionBody& program) {

    u32 jmp_inst_idx = emit(InstType::jmp);

    // generate bytecode for functions first, then record its function meta index in the map.
    for (auto& [func, init_code] : scope().get_inner_func_init_code()) {
      gen_func_bytecode(*func, init_code);
    }
    // skip function bytecode
    bytecode[jmp_inst_idx].operand.two.opr1 = bytecode_pos();

    // Fill the function symbol initialization code to the very beginning
    // (because the function variable will be hoisted)
    for (auto& inst : scope().get_context().function_init_code) {
      bytecode.push_back(inst);
    }
    scope().get_context().function_init_code.clear();

    for (ASTNode *node : program.func_decls) {
      auto *func = static_cast<Function *>(node);
      visit_function(*func);
      auto symbol = scope().resolve_symbol(func->name.text);
      emit(InstType::pop, scope_type_int(symbol.scope_type), symbol.index);
    }
    // End filling function symbol initialization code

    for (ASTNode *node : program.stmts) {
      visit(node);
      if (node->type > ASTNode::BEGIN_EXPR && node->type < ASTNode::END_EXPR
          && node->type != ASTNode::AST_EXPR_ASSIGN) {
        emit(InstType::pop_drop);
      }
    }

    if (program.type == ASTNode::AST_PROGRAM) emit(InstType::halt);
    else {
      if (bytecode[bytecode_pos() - 1].op_type != InstType::ret) {
        emit(InstType::push_undef);
        emit(InstType::ret);
      }
    }
  }

  void gen_func_bytecode(Function& func, SmallVector<Instruction, 5>& init_code) {

    ProgramOrFunctionBody *body = func.body->as_func_body();

    // create metadata for function
    u32 func_idx = add_function_meta( JSFunctionMeta {
        .name_index = func.has_name() ? add_const(func.name.text) : 0,
        .is_anonymous = !func.has_name() || func.is_arrow_func,
        .is_arrow_func = func.is_arrow_func,
        .param_count = (u16)func.params.size(),
        .local_var_count = (u16)body->scope->get_var_count(),
        .code_address = bytecode_pos(),
    });

    push_scope(std::move(body->scope));
    // generate bytecode for function body
    // Only after visiting the body do we know which variables are captured
    visit_program_or_function_body(*body);
    auto capture_list = std::move(scope().get_capture_list());

    // The rest of the work is done in the scope of the outer function, so pop scope here.
    pop_scope();

    // let the VM make a function
    init_code.emplace_back(InstType::make_func, func_idx);

    // capture closure variables
    for (auto symbol : capture_list) {
      init_code.emplace_back(InstType::capture, scope_type_int(symbol.scope_type),
                             symbol.original_symbol->index);
    }
  }

  void visit_function(Function& func) {
    auto& init_code = scope().get_inner_func_init_code()[&func];
    for (auto& inst : init_code) {
      emit(inst);
    }
  }

  void visit_unary_expr(UnaryExpr& expr) {

    switch (expr.op.type) {
      case Token::LOGICAL_NOT:
        if (expr.is_prefix_op) {
          visit(expr.operand);
          emit(InstType::logi_not);
        }
        else {
          assert(false);
        }
        break;
      case Token::SUB:
        if (expr.is_prefix_op) {
          visit(expr.operand);
          emit(InstType::neg);
        }
        else {
          assert(false);
        }
        break;
      default:
        break;
    }
  }

  void visit_binary_expr(BinaryExpr& expr) {

    if (expr.op.type == Token::ADD && expr.is_simple_expr()) {

      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());
      auto rhs_sym = scope().resolve_symbol(expr.rhs->get_source());

      emit(InstType::fast_add, scope_type_int(lhs_sym.scope_type), lhs_sym.index,
           scope_type_int(rhs_sym.scope_type), rhs_sym.index);
      return;
    }

    if (expr.op.is_binary_logical()) {
      vector<u32> true_list;
      vector<u32> false_list;
      visit_logical_expr(expr, true_list, false_list, true);

      for (u32 idx : true_list) {
        bytecode[idx].operand.two.opr1 = int(bytecode_pos());
      }
      for (u32 idx : false_list) {
        bytecode[idx].operand.two.opr1 = int(bytecode_pos());
      }
      return;
    }

    visit(expr.lhs);
    visit(expr.rhs);
    switch (expr.op.type) {
      case Token::ADD: emit(InstType::add); break;
      case Token::SUB: emit(InstType::sub); break;
      case Token::MUL: emit(InstType::mul); break;
      case Token::DIV: emit(InstType::div); break;
      case Token::EQ3: emit(InstType::eq3); break;
      case Token::NE3: emit(InstType::ne3); break;
      case Token::LE: emit(InstType::le); break;
      case Token::LT: emit(InstType::lt); break;
      case Token::GT: emit(InstType::gt); break;
      case Token::GE: emit(InstType::ge); break;
      default: assert(false);
    }

  }

  void visit_expr_in_logical_expr(ASTNode& expr, vector<u32>& true_list, vector<u32>& false_list, bool need_value) {
    if (expr.is_binary_logical_expr()) {
      visit_logical_expr(*expr.as_binary_expr(), true_list, false_list, need_value);
      return;
    }
    else if (expr.is_not_expr() && !need_value) {
      auto *not_expr = static_cast<UnaryExpr *>(&expr);
      visit_expr_in_logical_expr(*not_expr->operand, true_list, false_list, need_value);

      vector<u32> temp = std::move(false_list);
      false_list = std::move(true_list);
      true_list = std::move(temp);
      return;
    }

    visit(&expr);
    true_list.push_back(bytecode_pos());
    emit(InstType::jmp_true);
    false_list.push_back(bytecode_pos());
    emit(InstType::jmp);
  }

  void visit_logical_expr(BinaryExpr& expr, vector<u32>& true_list, vector<u32>& false_list, bool need_value) {

    vector<u32> lhs_true_list;
    vector<u32> lhs_false_list;
    visit_expr_in_logical_expr(*(expr.lhs), lhs_true_list, lhs_false_list, need_value);
    
    bool is_OR = expr.op.type == Token::LOGICAL_OR;
    if (is_OR) {
      // backpatch( B1.falselist, M.instr)
      for (u32 idx : lhs_false_list) {
        bytecode[idx].operand.two.opr1 = int(bytecode_pos());
      }
      true_list.insert(true_list.end(), lhs_true_list.begin(), lhs_true_list.end());
      emit(InstType::pop_drop);
    }
    else {
      for (u32 idx : lhs_true_list) {
        bytecode[idx].operand.two.opr1 = int(bytecode_pos());
      }
      false_list.insert(false_list.end(), lhs_false_list.begin(), lhs_false_list.end());
      emit(InstType::pop_drop);
    }

    vector<u32> rhs_true_list;
    vector<u32> rhs_false_list;
    visit_expr_in_logical_expr(*(expr.rhs), rhs_true_list, rhs_false_list, need_value);
    // If the operator is OR, we take the rhs's false list as this expression's false list.
    if (is_OR) false_list = std::move(rhs_false_list);
    else true_list = std::move(rhs_true_list);

    // merge the true list.
    if (is_OR) true_list.insert(true_list.end(), rhs_true_list.begin(), rhs_true_list.end());
    else false_list.insert(false_list.end(), rhs_false_list.begin(), rhs_false_list.end());
  }

  void visit_assignment_expr(AssignmentExpr& expr) {

    if (expr.is_simple_assign()) {
      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());
      auto rhs_sym = scope().resolve_symbol(expr.rhs->get_source());

      if (lhs_sym.stack_scope() && rhs_sym.stack_scope()) {
        emit(InstType::fast_assign, scope_type_int(lhs_sym.scope_type), lhs_sym.index,
             scope_type_int(rhs_sym.scope_type), rhs_sym.index);
      }
      else {

      }
    }
    else if (expr.lhs_is_id()) {
      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());

      if (expr.assign_type == TokenType::ASSIGN) {
        visit(expr.rhs);
        emit(InstType::pop, scope_type_int(lhs_sym.scope_type), (int)lhs_sym.index);
      }
      else if (expr.assign_type == TokenType::ADD_ASSIGN) {
        if (expr.rhs->as_number_literal() && expr.rhs->as_number_literal()->num_val == 1) {
          emit(InstType::inc, scope_type_int(lhs_sym.scope_type), (int)lhs_sym.index);
        } else {
          visit(expr.rhs);
          emit(InstType::add_assign, scope_type_int(lhs_sym.scope_type), (int)lhs_sym.index);
        }
      }
      else assert(false);
      
    }
    else {
      // check if left hand side is LeftHandSide Expression (or Parenthesized Expression with
      // LeftHandSide Expression in it.
      if (expr.lhs->type == ASTNode::AST_EXPR_LHS) {
        visit_left_hand_side_expr(*static_cast<LeftHandSideExpr*>(expr.lhs), true);
      }
      else assert(false);

      visit(expr.rhs);
      emit(InstType::prop_assign);
    }
  }

  void visit_object_literal(ObjectLiteral& obj_lit) {
    emit(InstType::make_obj);

    for (auto& prop : obj_lit.properties) {
      // push the key into the stack
      emit(InstType::push_atom, (int)add_const(prop.key.text));

      // push the value into the stack
      visit(prop.value);
    }

    if (!obj_lit.properties.empty()) {
      emit(InstType::add_props, (int)obj_lit.properties.size());
    }
  }

  void visit_array_literal(ArrayLiteral& array_lit) {
    emit(InstType::make_array);

    for (auto& [idx, element] : array_lit.elements) {
      visit(element);
    }

    if (!array_lit.elements.empty()) {
      emit(InstType::add_elements, (int)array_lit.elements.size());
    }
  }

  void visit_left_hand_side_expr(LeftHandSideExpr& expr, bool create_ref) {
    visit(expr.base);
    auto& postfix_ord = expr.postfix_order;
    size_t postfix_size = expr.postfix_order.size();

    for (size_t i = 0; i < postfix_size; i++) {
      auto postfix_type = postfix_ord[i].first;
      u32 idx = postfix_ord[i].second;
      // obj.func()
      if (postfix_type == LeftHandSideExpr::CALL) {
        visit_func_arguments(*(expr.args_list[idx]));
        bool has_this_object = i > 0 && postfix_ord[i - 1].first != LeftHandSideExpr::CALL;
        emit(InstType::call, expr.args_list[idx]->args.size(), int(has_this_object));
      }
      // obj.prop
      else if (postfix_type == LeftHandSideExpr::PROP) {
        size_t prop_start = i;
        for (; postfix_ord[i].first == LeftHandSideExpr::PROP && i < postfix_size; i++) {
          idx = postfix_ord[i].second;
          int keypath_id = (int)add_const(expr.prop_list[idx].text);
          emit(InstType::push_atom, keypath_id);
        }

        emit(InstType::keypath_access, int(i - prop_start), int(create_ref));
        // Both the for loop above and the outer for loop have `+1` at the end. so minus one here.
        i -= 1;
      }
      // obj[prop]
      else if (postfix_type == LeftHandSideExpr::INDEX) {
        // evaluate the index expression
        assert(expr.index_list[idx]->is_expression());
        visit(expr.index_list[idx]);
        emit(InstType::index_access, int(create_ref && i == postfix_size - 1));
      }
    }
  }

  void visit_identifier(ASTNode& id) {
    auto symbol = scope().resolve_symbol(id.get_source());
    assert(!symbol.not_found());
    emit(InstType::push, scope_type_int(symbol.scope_type), symbol.index);
  }

  void visit_func_arguments(Arguments& args) {
    for (auto node : args.args) {
      visit(node);
    }
  }

  void visit_number_literal(NumberLiteral& node) {
    emit(Instruction::num_imm(node.num_val));
  }

  void visit_string_literal(ASTNode& node) {
    auto str_view = node.get_source();
    str_view.remove_prefix(1);
    str_view.remove_suffix(1);
    emit(InstType::push_str, (int)add_const(str_view));
  }

  void visit_variable_statement(VarStatement& var_stmt) {
    for (VarDecl *decl : var_stmt.declarations) {
        visit_variable_declaration(*decl);
    }
  }

  void visit_variable_declaration(VarDecl& var_decl) {
    scope().mark_symbol_as_valid(var_decl.id.text);
    if (var_decl.var_init) visit(var_decl.var_init);
  }

  void visit_return_statement(ReturnStatement& return_stmt) {
    if (return_stmt.expr) {
        visit(return_stmt.expr);
    } else {
        emit(InstType::push_undef);
    }
    emit(InstType::ret);
  }

  void visit_if_statement(IfStatement& stmt) {
    vector<u32> true_list;
    vector<u32> false_list;
    visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);
    for (u32 idx : true_list) {
        bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    emit(InstType::pop_drop);
    visit(stmt.if_block);
    u32 if_end_jmp;
    if_end_jmp = emit(InstType::jmp);

    for (u32 idx : false_list) {
        bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    if (stmt.else_block) {
      visit(stmt.else_block);
    }
    bytecode[if_end_jmp].operand.two.opr1 = bytecode_pos();
  }

  void visit_while_statement(WhileStatement& stmt) {
    vector<u32> true_list;
    vector<u32> false_list;
    u32 loop_start = bytecode_pos();
    scope().get_context().continue_pos = loop_start;
    scope().get_context().can_break = true;

    visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);
    for (u32 idx : true_list) {
        bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    emit(InstType::pop_drop);
    visit(stmt.body_stmt);
    emit(InstType::jmp, loop_start);

    for (u32 idx : false_list) {
        bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    for (u32 idx : scope().get_context().break_list) {
        bytecode[idx].operand.two.opr1 = bytecode_pos();
    }

    scope().get_context().can_break = false;
  }

  void visit_continue_break_statement(ContinueOrBreak& stmt) {
    if (stmt.type == ASTNode::AST_STMT_CONTINUE) {
        int64_t continue_pos = scope().resolve_continue_pos();
        assert(continue_pos != -1);
        emit(InstType::jmp, u32(continue_pos));
    }
    else {
        scope().resolve_break_list()->push_back(bytecode_pos());
        emit(InstType::jmp);
    }
  }

  void visit_block_statement(Block& block) {
    push_scope(std::move(block.scope));

    for (auto *stmt : block.statements) {
      visit(stmt);
      if (stmt->type > ASTNode::BEGIN_EXPR && stmt->type < ASTNode::END_EXPR
          && stmt->type != ASTNode::AST_EXPR_ASSIGN) {
        emit(InstType::pop_drop);
      }
    }
    pop_scope();
  }

 private:
  /// Get current scope.
  Scope& scope() { return *scope_chain.back(); }

  void push_scope(ScopeType scope_type) {
    Scope *outer = !scope_chain.empty() ? scope_chain.back().get() : nullptr;
    scope_chain.emplace_back(std::make_unique<Scope>(scope_type, outer));
  }

  void push_scope(unique_ptr<Scope> scope) {
    Scope *outer = !scope_chain.empty() ? scope_chain.back().get() : nullptr;
    scope->set_outer(outer);
    scope_chain.emplace_back(std::move(scope));
  }

  void pop_scope() {
    scope_chain.pop_back();
  }

  template <typename... Args>
  u32 emit(Args &&...args) {
    bytecode.emplace_back(std::forward<Args>(args)...);
    return bytecode.size() - 1;
  }

  u32 emit(Instruction inst) {
    bytecode.push_back(inst);
    return bytecode.size() - 1;
  }

  void report_error(CodegenError err) {
    error.push_back(std::move(err));
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

  void add_builtin_functions() {
    assert(scope().get_scope_type() == ScopeType::GLOBAL);
    auto& func_init_code = scope().get_context().function_init_code;

    for (auto& [name, record] : scope().get_symbol_table()) {
      if (record.is_builtin && record.var_kind == VarKind::DECL_FUNCTION) {
        u32 meta_idx = add_function_meta(JSFunctionMeta{
            .name_index = add_const(name),
            .is_native = true,
            .param_count = 0,
            .local_var_count = 0,
            .native_func = nullptr,
        });
        func_init_code.emplace_back(InstType::make_func, int(meta_idx));
        func_init_code.emplace_back(InstType::pop, scope_type_int(ScopeType::GLOBAL), (int)record.index);
      }
    }
  }


  std::vector<unique_ptr<Scope>> scope_chain;
  std::vector<Instruction> bytecode;
  SmallVector<CodegenError, 10> error;

  // for constant
  StringPool str_pool;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  // for atom
  SmallVector<u16string, 10> atom_pool;

};

} // namespace njs

#endif // NJS_CODEGEN_VISITOR_H