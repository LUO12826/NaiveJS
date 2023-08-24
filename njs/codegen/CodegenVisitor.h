#ifndef NJS_CODEGEN_VISITOR_H
#define NJS_CODEGEN_VISITOR_H

#include "Scope.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/common/StringPool.h"
#include "njs/common/enums.h"
#include "njs/include/SmallVector.h"
#include "njs/include/robin_hood.h"
#include "njs/parser/ast.h"
#include "njs/utils/Timer.h"
#include "njs/utils/helper.h"
#include "njs/vm/Instructions.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace njs {

using robin_hood::unordered_map;
using std::u16string;
using std::unique_ptr;
using std::vector;
using u32 = uint32_t;

struct CodegenError {
  std::string message;
  ASTNode *ast_node;
};

class CodegenVisitor {
  friend class NjsVM;

 public:
  unordered_map<u16string, u32> global_props_map;

  void codegen(ProgramOrFunctionBody *prog) {

    push_scope(prog->scope.get());

    // add functions to the global scope
    add_builtin_functions();
    visit_program_or_function_body(*prog);

    Timer timer("optimized");
    optimize();
    timer.end();

    if (!Global::show_codegen_result) return;
    std::cout << "================ codegen result ================\n\n";

    std::cout << ">>> instructions:\n";
    for (int i = 0; i < bytecode.size(); i++) {
      std::cout << std::setw(6) << std::left << i << bytecode[i].description() << '\n';
    }
    std::cout << '\n';

    std::cout << ">>> string pool:\n";
    std::vector<u16string>& str_list = str_pool.get_string_list();

    for (int i = 0; i < str_list.size(); i++) {
      std::cout << std::setw(3) << i << " " << to_utf8_string(str_list[i]) << '\n';
    }
    std::cout << '\n';

    std::cout << ">>> number pool:\n";
    for (int i = 0; i < num_list.size(); i++) {
      std::cout << std::setw(3) << i << " " << num_list[i] << '\n';
    }
    std::cout << '\n';

    std::cout << ">>> function metadata:\n";
    for (int i = 0; i < func_meta.size(); i++) {
      auto& meta = func_meta[i];
      std::string func_name = meta.is_anonymous ? "(anonymous)"
                                                : to_utf8_string(str_list[meta.name_index]);
      std::cout << "index: " << std::setw(3) << i << ", name: " << std::setw(30)
                << func_name << " addr: " << meta.code_address
                << '\n';
    }
    std::cout << '\n';
    std::cout << "============== end codegen result ==============\n\n";
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

        if (inst.op_type == InstType::jmp && prev_inst.op_type == InstType::jmp_true &&
            prev_inst.operand.two.opr1 == i + 1) {

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
        if (inst.op_type == InstType::jmp || inst.op_type == InstType::jmp_true ||
            inst.op_type == InstType::jmp_false) {
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

      for (auto& entry : meta.catch_table) {
        entry.goto_pos += pos_moved[entry.goto_pos];
      }
    }

    for (auto& entry : scope_chain[0]->get_context().catch_table) {
      entry.goto_pos += pos_moved[entry.goto_pos];
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
      case ASTNode::AST_EXPR_NUMBER:
        visit_number_literal(*static_cast<NumberLiteral *>(node));
        break;
      case ASTNode::AST_EXPR_STRING:
        visit_string_literal(*static_cast<StringLiteral *>(node));
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
        assert(false);
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
        visit_block_statement(*static_cast<Block *>(node), {});
        break;
      case ASTNode::AST_STMT_THROW:
        visit_throw_statement(*static_cast<ThrowStatement *>(node));
        break;
      case ASTNode::AST_STMT_TRY:
        visit_try_statement(*static_cast<TryStatement *>(node));
        break;
      default:
        std::cout << node->description() << " not supported yet" << std::endl;
        assert(false);
    }
  }

  void visit_program_or_function_body(ProgramOrFunctionBody& program) {

    // First, allocate space for the variables in this scope
    for (ASTNode *node : program.statements) {
      if (node->type != ASTNode::AST_STMT_VAR) continue;

      auto& var_stmt = *static_cast<VarStatement *>(node);
      for (VarDecl *decl : var_stmt.declarations) {
        if (var_stmt.kind == VarKind::DECL_LET || var_stmt.kind == VarKind::DECL_CONST) {
          bool res = scope().define_symbol(var_stmt.kind, decl->id.text);
          if (!res) {
            report_error(CodegenError {
                .message = "Duplicate variable: " + to_utf8_string(decl->id.text),
                .ast_node = decl,
            });
          }
        }
      }
    }

    // begin codegen for inner functions
    if (!scope().get_inner_func_init_code().empty()) {
      u32 jmp_inst_idx = emit(InstType::jmp);

      // generate bytecode for functions first, then record its function meta index in the map.
      for (auto& [func, init_code] : scope().get_inner_func_init_code()) {
        gen_func_bytecode(*func, init_code);
      }
      // skip function bytecode
      bytecode[jmp_inst_idx].operand.two.opr1 = bytecode_pos();

      for (ASTNode *node : program.func_decls) {
        auto *func = static_cast<Function *>(node);
        visit_function(*func);
        auto symbol = scope().resolve_symbol(func->name.text);
        assert(!symbol.not_found());
        emit(InstType::pop, scope_type_int(symbol.storage_scope), symbol.get_index());
      }
      // End filling function symbol initialization code
    }

    std::vector<u32> top_level_throw;

    for (ASTNode *node : program.statements) {
      visit(node);
      if (node->type > ASTNode::BEGIN_EXPR && node->type < ASTNode::END_EXPR &&
          node->type != ASTNode::AST_EXPR_ASSIGN) {
        emit(InstType::pop_drop);
      }
      if (scope().has_context()) {
        auto& throw_list = scope().get_context().throw_list;
        top_level_throw.insert(top_level_throw.end(), throw_list.begin(), throw_list.end());
        throw_list.clear();
      }
    }

    if (program.type == ASTNode::AST_PROGRAM) {
      emit(InstType::halt);
    }
    else { // function body
      if (bytecode[bytecode_pos() - 1].op_type != InstType::ret) {
        emit(InstType::push_undef);
        emit(InstType::ret);
      }
    }

    scope().get_outer_func()->get_context().catch_table.emplace_back(0, 0, bytecode_pos());

    for (u32 idx : top_level_throw) {
      bytecode[idx].operand.two.opr1 = bytecode_pos();
    }

    if (program.type == ASTNode::AST_PROGRAM) {
      emit(InstType::halt_err);
    }
    else {
      emit(InstType::ret_err);
    }
  }

  void gen_func_bytecode(Function& func, SmallVector<Instruction, 5>& init_code) {

    ProgramOrFunctionBody *body = func.body->as_func_body();
    u32 func_start_pos = bytecode_pos();
    push_scope(body->scope.get());

    // generate bytecode for function body
    visit_program_or_function_body(*body);

    // Only after visiting the body do we know which variables are captured
    auto capture_list = std::move(scope().get_capture_list());

    // create metadata for function
    u32 func_idx = add_function_meta( JSFunctionMeta {
        .name_index = func.has_name() ? add_const(func.name.text) : 0,
        .is_anonymous = !func.has_name() || func.is_arrow_func,
        .is_arrow_func = func.is_arrow_func,
        .param_count = (u16)scope().get_param_count(),
        .local_var_count = (u16)scope().get_var_count(),
        .code_address = func_start_pos,
        .source_line = func.get_line_start(),
        .catch_table = std::move(scope().get_context().catch_table)
    });
    // The rest of the work is done in the scope of the outer function, so pop scope here.
    pop_scope();

    // let the VM make a function
    init_code.emplace_back(InstType::make_func, func_idx);

    // capture closure variables
    for (auto symbol : capture_list) {
      init_code.emplace_back(InstType::capture, scope_type_int(symbol.storage_scope),
                             symbol.get_index());
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

      emit(InstType::fast_add, scope_type_int(lhs_sym.storage_scope), lhs_sym.get_index(),
           scope_type_int(rhs_sym.storage_scope), rhs_sym.get_index());
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

  void visit_expr_in_logical_expr(ASTNode& expr, vector<u32>& true_list, vector<u32>& false_list,
                                  bool need_value) {
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

  void visit_logical_expr(BinaryExpr& expr, vector<u32>& true_list, vector<u32>& false_list,
                          bool need_value) {

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
    // a = b
    if (expr.is_simple_assign()) {
      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());
      auto rhs_sym = scope().resolve_symbol(expr.rhs->get_source());

      emit(InstType::fast_assign, scope_type_int(lhs_sym.storage_scope), lhs_sym.get_index(),
           scope_type_int(rhs_sym.storage_scope), rhs_sym.get_index());
    }
    // a = ... or a += ...
    else if (expr.lhs_is_id()) {
      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());
      int lhs_scope = scope_type_int(lhs_sym.storage_scope);
      int lhs_sym_index = (int)lhs_sym.get_index();
      // a = ...
      if (expr.assign_type == TokenType::ASSIGN) {
        visit(expr.rhs);
        emit(InstType::pop, lhs_scope, lhs_sym_index);
      }
      // a += ...
      else if (expr.assign_type == TokenType::ADD_ASSIGN) {
        if (expr.rhs->is(ASTNode::AST_EXPR_NUMBER) && expr.rhs->as_number_literal()->num_val == 1) {
          emit(InstType::inc, lhs_scope, lhs_sym_index);
        }
        else {
          visit(expr.rhs);
          emit(InstType::add_assign, lhs_scope, lhs_sym_index);
        }
      }
      // a -= ...
      else if (expr.assign_type == TokenType::SUB_ASSIGN) {
        if (expr.rhs->is(ASTNode::AST_EXPR_NUMBER) && expr.rhs->as_number_literal()->num_val == 1) {
          emit(InstType::dec, lhs_scope, lhs_sym_index);
        }
        else {
          visit(expr.rhs);
          emit(InstType::sub_assign, lhs_scope, lhs_sym_index);
        }
      }
      else {
        visit(expr.rhs);
        switch (expr.assign_type) {
          case TokenType::MUL_ASSIGN:
            emit(InstType::mul_assign, lhs_scope, lhs_sym_index);
            break;
          case TokenType::DIV_ASSIGN:
            emit(InstType::div_assign, lhs_scope, lhs_sym_index);
            break;
        }
      }
    }
    else {
      // check if left hand side is LeftHandSide Expression (or Parenthesized Expression with
      // LeftHandSide Expression in it.
      if (expr.lhs->type == ASTNode::AST_EXPR_LHS) {
        visit_left_hand_side_expr(*static_cast<LeftHandSideExpr *>(expr.lhs), true);
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
    emit(InstType::make_array, (int)array_lit.elements.size());

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
    emit(InstType::push, scope_type_int(symbol.storage_scope), symbol.get_index());
  }

  void visit_func_arguments(Arguments& args) {
    for (auto node : args.args) {
      visit(node);
    }
  }

  void visit_number_literal(NumberLiteral& node) { emit(Instruction::num_imm(node.num_val)); }

  void visit_string_literal(StringLiteral& node) {
    auto& str = node.str_val;
    emit(InstType::push_str, (int)add_const(str));
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
    }
    else {
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

    if (is_stmt_valid_in_single_ctx(stmt.if_block)) {
      visit(stmt.if_block);
    }
    else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.if_block,
      });
    }

    u32 if_end_jmp;
    if_end_jmp = emit(InstType::jmp);

    for (u32 idx : false_list) {
      bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    if (stmt.else_block) {
      if (is_stmt_valid_in_single_ctx(stmt.else_block)) {
        visit(stmt.else_block);
      }
      else {
        report_error(CodegenError {
            .message =
                "SyntaxError: Lexical declaration cannot appear in a single-statement context",
            .ast_node = stmt.else_block,
        });
      }
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

    if (is_stmt_valid_in_single_ctx(stmt.body_stmt)) {
      visit(stmt.body_stmt);
    }
    else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.body_stmt,
      });
    }
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

  // If `extra_var` is not empty, it means that the external wants to define some variables
  // in advance in this block.
  void visit_block_statement(Block& block, const vector<pair<u16string_view, VarKind>>& extra_var) {
    push_scope(block.scope.get());

    // First, allocate space for the variables in this scope
    for (auto& [name, kind] : extra_var) {
      assert(kind == VarKind::DECL_LET || kind == VarKind::DECL_CONST);
      bool res = scope().define_symbol(kind, name);
      scope().mark_symbol_as_valid(name);
      if (!res) std::cout << "!!!!define symbol " << to_utf8_string(name) << " failed\n";
    }

    for (ASTNode *node : block.statements) {
      if (node->type != ASTNode::AST_STMT_VAR) continue;

      auto& var_stmt = *static_cast<VarStatement *>(node);
      for (VarDecl *decl : var_stmt.declarations) {
        if (var_stmt.kind == VarKind::DECL_LET || var_stmt.kind == VarKind::DECL_CONST) {
          bool res = scope().define_symbol(var_stmt.kind, decl->id.text);
          if (!res) std::cout << "!!!!define symbol " << decl->id.get_text_utf8() << " failed\n";
        }
      }
    }

    // begin codegen for inner functions
    if (!scope().get_inner_func_init_code().empty()) {
      u32 jmp_inst_idx = emit(InstType::jmp);

      // generate bytecode for functions first, then record its function meta index in the map.
      for (auto& [func, init_code] : scope().get_inner_func_init_code()) {
        gen_func_bytecode(*func, init_code);
      }
      // skip function bytecode
      bytecode[jmp_inst_idx].operand.two.opr1 = bytecode_pos();

      for (ASTNode *node : block.statements) {
        if (node->type != ASTNode::AST_FUNC) continue;

        auto *func = static_cast<Function *>(node);
        visit_function(*func);
        auto symbol = scope().resolve_symbol(func->name.text);
        assert(!symbol.not_found());
        emit(InstType::pop, scope_type_int(symbol.storage_scope), symbol.get_index());
      }
      // End filling function symbol initialization code
    }

    for (auto *stmt : block.statements) {
      visit(stmt);
      if (stmt->type > ASTNode::BEGIN_EXPR && stmt->type < ASTNode::END_EXPR &&
          stmt->type != ASTNode::AST_EXPR_ASSIGN) {
        emit(InstType::pop_drop);
      }
    }

    // dispose the scope variables after leaving the block scope
    gen_scope_var_dispose_code(scope());
    pop_scope();
  }

  void gen_scope_var_dispose_code(Scope& the_scope) {
    auto& sym_table = the_scope.get_symbol_table();
    for (auto& [name, sym_rec] : sym_table) {
      if (sym_rec.var_kind == VarKind::DECL_VAR || sym_rec.var_kind == VarKind::DECL_FUNC_PARAM ||
          sym_rec.var_kind == VarKind::DECL_FUNCTION) {
        assert(false);
      }
      auto scope_type = the_scope.get_outer_func()->get_scope_type();
      emit(InstType::var_dispose, scope_type_int(scope_type), sym_rec.offset_idx());
    }
  }

  void visit_try_statement(TryStatement& stmt) {
    assert(stmt.try_block->type == ASTNode::AST_STMT_BLOCK);
    scope().get_context().has_try = true;

    // visit the try block to emit bytecode
    u32 try_start = bytecode_pos();
    visit(stmt.try_block);
    u32 try_end = bytecode_pos() - 1;

    // We are going to record the address of the variables in the try block.
    // When an exception happens, we should dispose the variables in this block.
    u32 var_dispose_start = scope().get_next_var_index() + frame_meta_size;
    u32 var_dispose_end = stmt.try_block->as_block()->scope->get_var_count() + frame_meta_size;

    scope().get_context().has_try = false;

    u32 try_end_jmp = emit(InstType::jmp);
    u32 catch_pos = try_end_jmp + 1;

    // `throw` statements will jump here
    for (u32 idx : scope().get_context().throw_list) {
      bytecode[idx].operand.two.opr1 = int(catch_pos);
    }
    scope().get_context().throw_list.clear();
    // also, any error in the try block will jump here. So we should add a catch table entry.
    auto& catch_table = scope().get_outer_func()->get_context().catch_table;
    catch_table.emplace_back(try_start, try_end, catch_pos);
    catch_table.back().var_dispose_start = var_dispose_start;
    catch_table.back().var_dispose_end = var_dispose_end;

    // in the case of `catch (a) ...`, we are going to store the top-of-stack value to a local
    // variable. The variable is defined as `let`.
    if (stmt.catch_ident.is(TokenType::IDENTIFIER)) {
      // 2 for the stack frame metadata size. Should find a better way to write this.
      int catch_id_addr = scope().get_next_var_index() + frame_meta_size;
      emit(InstType::pop, scope_type_int(scope().get_outer_func()->get_scope_type()),
           catch_id_addr);
    }
    else {
      emit(InstType::pop_drop);
    }

    auto *catch_block = static_cast<Block *>(stmt.catch_block);

    vector<pair<u16string_view, VarKind>> catch_id;
    if (stmt.catch_ident.is(TokenType::IDENTIFIER)) {
      catch_id.emplace_back(stmt.catch_ident.text, VarKind::DECL_LET);
    }
    visit_block_statement(*catch_block, catch_id);
    bytecode[try_end_jmp].operand.two.opr1 = bytecode_pos();
  }

  void visit_throw_statement(ThrowStatement& stmt) {
    assert(stmt.expr->is_expression());
    visit(stmt.expr);
    emit(InstType::dup_stack_top);

    Scope *scope_to_clean = &scope();
    while (true) {
      bool should_clean = scope_to_clean->get_scope_type() != ScopeType::GLOBAL &&
                          scope_to_clean->get_scope_type() != ScopeType::FUNC &&
                          !scope_to_clean->has_try();
      if (!should_clean) break;

      gen_scope_var_dispose_code(*scope_to_clean);
      scope_to_clean = scope_to_clean->get_outer();
    }

    scope().resolve_throw_list()->push_back(bytecode_pos());
    emit(InstType::jmp);
  }

 private:
  /// Get current scope.
  Scope& scope() { return *scope_chain.back(); }

  void push_scope(Scope *scope) {
    Scope *outer = !scope_chain.empty() ? scope_chain.back() : nullptr;
    scope->set_outer(outer);
    scope_chain.emplace_back(scope);
  }

  void pop_scope() {
    Scope *scope = scope_chain.back();
    scope_chain.pop_back();
    if (scope->get_outer_func()) {
      scope->get_outer_func()->update_var_count(scope->get_next_var_index());
    }
  }

  template <typename... Args>
  u32 emit(Args&&...args) {
    bytecode.emplace_back(std::forward<Args>(args)...);
    return bytecode.size() - 1;
  }

  u32 emit(Instruction inst) {
    bytecode.push_back(inst);
    return bytecode.size() - 1;
  }

  void report_error(CodegenError err) { error.push_back(std::move(err)); }

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

  u32 bytecode_pos() { return bytecode.size(); }

  bool is_stmt_valid_in_single_ctx(ASTNode *stmt) {
    if (stmt->is(ASTNode::AST_STMT_VAR)) {
      auto *var_stmt = static_cast<VarStatement *>(stmt);
      if (var_stmt->kind != VarKind::DECL_VAR) return false;
    }
    return true;
  }

  void add_builtin_functions() {
    assert(scope().get_scope_type() == ScopeType::GLOBAL);

    for (auto& [name, record] : scope().get_symbol_table()) {
      if (record.is_builtin && record.var_kind == VarKind::DECL_FUNCTION) {
        u32 meta_idx = add_function_meta(JSFunctionMeta {
            .name_index = add_const(name),
            .is_native = true,
            .param_count = 0,
            .local_var_count = 0,
            .native_func = nullptr,
        });
        emit(InstType::make_func, int(meta_idx));
        emit(InstType::pop, scope_type_int(ScopeType::GLOBAL), (int)record.offset_idx());
      }
    }
  }

  static constexpr u32 frame_meta_size {2};

  std::vector<Scope *> scope_chain;
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