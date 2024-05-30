#ifndef NJS_CODEGEN_VISITOR_H
#define NJS_CODEGEN_VISITOR_H

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <functional>
#include "Scope.h"
#include "njs/global_var.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/common/AtomPool.h"
#include "njs/common/enums.h"
#include "njs/common/common_def.h"
#include "njs/common/JSErrorType.h"
#include "njs/include/SmallVector.h"
#include "njs/include/robin_hood.h"
#include "njs/parser/Token.h"
extern "C" {
#include "njs/include/libregexp/libregexp.h"
}
#include "njs/parser/ast.h"
#include "njs/utils/Timer.h"
#include "njs/utils/helper.h"
#include "njs/vm/Instruction.h"

namespace njs {

using robin_hood::unordered_map;
using std::u16string;
using std::vector;
using U32Pair = std::array<u32, 2>;
using u32 = uint32_t;

struct CodegenError {
  JSErrorType type;
  std::string message;
  ASTNode *ast_node;

  void describe() {
    printf("At line %u: %s: %s\n",
           ast_node->start_line_num(),
           to_u8string(native_error_name[type]).c_str(),
           message.c_str());
  }
};

class CodegenVisitor {
  friend class NjsVM;

 public:
  void codegen(ProgramOrFunctionBody *prog) {

    push_scope(prog->scope.get());
    visit_program_or_function_body(*prog);

    if (Global::enable_optimization) {
      Timer timer("optimized");
      optimize();
      timer.end();
    }

    check_bytecode();

    if (!Global::show_codegen_result) return;
    std::cout << "================ codegen result ================\n\n";

    std::cout << "[[instructions]]\n";
    for (int i = 0; i < bytecode.size(); i++) {
      std::cout << std::setw(10) << std::left << i << bytecode[i].description() << '\n';
    }
    std::cout << '\n';

    std::cout << "[[function metadata]]\n";
    for (int i = 0; i < func_meta.size(); i++) {
      auto& meta = func_meta[i];
      auto func_name = meta.is_anonymous ? "(anonymous)"
                                         : to_u8string(atom_pool.get_string(meta.name_index));
      std::cout << "index: " << std::setw(3) << i
                << " name: " << std::setw(18) << func_name
                << " source_line: " << std::setw(8) << meta.source_line
                << " num_local_var: " << std::setw(3) << meta.local_var_count
                << " stack_size: " << std::setw(3) << meta.stack_size
                << " bc_begin: " << meta.bytecode_start
                << " bc_end: " << meta.bytecode_end
                << '\n';

      std::cout << "catch table:\n";
      for (auto& catch_item : meta.catch_table) {
        std::cout << "  " << catch_item.description() << '\n';
      }
      std::cout << "\n";
    }
    std::cout << "global catch table:\n";
    for (auto& catch_item : scope_chain[0]->catch_table) {
      std::cout << "  " << catch_item.description() << '\n';
    }

    std::cout << "global info:\n";
    std::cout << " num_local_var: " << std::setw(4) << scope().get_var_count()
              << " stack_size: " << std::setw(3) << scope().get_max_stack_size()
              << '\n';

    std::cout << "\n";
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

      //      if (inst.op_type == OpType:: push && prev_inst.op_type == OpType::pop) {
      //        if (inst.operand.two[0] == prev_inst.operand.two[0]
      //            && inst.operand.two[1] == prev_inst.operand.two[1]) {
      //          removed_inst_cnt += 1;
      //          inst.op_type = OpType::nop;
      //          prev_inst.op_type = OpType::store;
      //        }
      //      }

      if (inst.op_type == OpType::jmp_cond
          && inst.operand.two[0] == inst.operand.two[1]
          && inst.operand.two[0] == i + 1) {
        removed_inst_cnt += 1;
        inst.op_type = OpType::nop;
      }

      if (inst.op_type == OpType::jmp_true || inst.op_type == OpType::jmp) {
        if (inst.operand.two[0] == i + 1) {
          removed_inst_cnt += 1;
          inst.op_type = OpType::nop;
        }

        if (inst.op_type == OpType::jmp && prev_inst.op_type == OpType::jmp_true &&
            prev_inst.operand.two[0] == i + 1) {

          prev_inst.op_type = OpType::jmp_false;
          prev_inst.operand.two[0] = inst.operand.two[0];
          inst.op_type = OpType::nop;
          removed_inst_cnt += 1;
        }
      }
    }

    size_t new_inst_ptr = 0;
    for (size_t i = 0; i < len; i++) {

      if (bytecode[i].op_type != OpType::nop) {
        auto& inst = bytecode[i];
        // Fix the jump target of the jmp instructions
        if (inst.is_jump_single_target()) {
          inst.operand.two[0] += pos_moved[inst.operand.two[0]];
        } else if (inst.is_jump_two_target()) {
          inst.operand.two[0] += pos_moved[inst.operand.two[0]];
          inst.operand.two[1] += pos_moved[inst.operand.two[1]];
        } else if (inst.op_type == OpType::proc_call) {
          inst.operand.two[0] += pos_moved[inst.operand.two[0]];
        }
        bytecode[new_inst_ptr] = inst;
        new_inst_ptr += 1;
      }
    }
    // Fix the jump target of the call instructions
    for (auto& meta : func_meta) {
      if (meta.is_native) continue;
      meta.bytecode_start += pos_moved[meta.bytecode_start];

      for (auto& entry : meta.catch_table) {
        entry.start_pos += pos_moved[entry.start_pos];
        entry.end_pos += pos_moved[entry.end_pos];
        entry.goto_pos += pos_moved[entry.goto_pos];
      }
    }

    for (auto& entry : scope_chain[0]->catch_table) {
      entry.start_pos += pos_moved[entry.start_pos];
      entry.end_pos += pos_moved[entry.end_pos];
      entry.goto_pos += pos_moved[entry.goto_pos];
    }

    bytecode.resize(new_inst_ptr);
  }

  void check_bytecode() {
    for (size_t i = 0; i < bytecode.size(); i++) {
      auto& bc = bytecode[i];

      if (bc.is_jump_two_target()) {
        assert(bc.operand.two[0] > 0 && bc.operand.two[1] > 0);
      }
      if (bc.is_jump_single_target()) {
        assert(bc.operand.two[0] > 0);
      }
    }
  }

  void visit(ASTNode *node) {
    switch (node->type) {
      case ASTNode::PROGRAM:
      case ASTNode::FUNC_BODY:
        visit_program_or_function_body(*static_cast<ProgramOrFunctionBody *>(node));
        break;
      case ASTNode::FUNC:
        visit_function(*static_cast<Function *>(node));
        break;
      case ASTNode::EXPR_BINARY:
        visit_binary_expr(*static_cast<BinaryExpr *>(node));
        break;
      case ASTNode::EXPR_UNARY:
        visit_unary_expr(*static_cast<UnaryExpr *>(node));
        break;
      case ASTNode::EXPR_ASSIGN:
        visit_assignment_expr(*static_cast<AssignmentExpr *>(node));
        break;
      case ASTNode::EXPR_LHS:
        visit_left_hand_side_expr(*static_cast<LeftHandSideExpr *>(node), false, false);
        break;
      case ASTNode::EXPR_ID:
        visit_identifier(*node);
        break;
      case ASTNode::EXPR_ARGS:
        visit_func_arguments(*static_cast<Arguments *>(node));
        break;
      case ASTNode::EXPR_NUMBER:
        visit_number_literal(*static_cast<NumberLiteral *>(node));
        break;
      case ASTNode::EXPR_STRING:
        visit_string_literal(*static_cast<StringLiteral *>(node));
        break;
      case ASTNode::EXPR_OBJ:
        visit_object_literal(*static_cast<ObjectLiteral *>(node));
        break;
      case ASTNode::EXPR_ARRAY:
        visit_array_literal(*static_cast<ArrayLiteral *>(node));
        break;
      case ASTNode::EXPR_NULL:
        emit(OpType::push_null);
        break;
      case ASTNode::EXPR_THIS:
        if (scope().get_outer_func()->get_type() == ScopeType::GLOBAL) {
          emit(OpType::push_global_this);
        } else {
          emit(OpType::push_func_this);
        }
        break;
      case ASTNode::EXPR_PAREN:
        visit(static_cast<ParenthesisExpr *>(node)->expr);
        break;
      case ASTNode::EXPR_BOOL:
        emit(OpType::push_bool, u32(node->get_source() == u"true"));
        break;
      case ASTNode::STMT_VAR:
        visit_variable_statement(*static_cast<VarStatement *>(node), true);
        break;
      case ASTNode::STMT_VAR_DECL:
        assert(false);
      case ASTNode::STMT_RETURN:
        visit_return_statement(*static_cast<ReturnStatement *>(node));
        break;
      case ASTNode::STMT_IF:
        visit_if_statement(*static_cast<IfStatement *>(node));
        break;
      case ASTNode::STMT_WHILE:
        visit_while_statement(*static_cast<WhileStatement *>(node));
        break;
      case ASTNode::STMT_CONTINUE:
      case ASTNode::STMT_BREAK:
        visit_continue_break_statement(*static_cast<ContinueOrBreak *>(node));
        break;
      case ASTNode::STMT_BLOCK:
        node->as_block()->scope->associated_node = node;
        node->as_block()->scope->set_block_type(BlockType::PLAIN);
        visit_block_statement(*static_cast<Block *>(node), true, _, _, _);
        break;
      case ASTNode::STMT_THROW:
        visit_throw_statement(*static_cast<ThrowStatement *>(node));
        break;
      case ASTNode::STMT_TRY:
        visit_try_statement(*static_cast<TryStatement *>(node));
        break;
      case ASTNode::EXPR_NEW:
        visit_new_expr(*static_cast<NewExpr *>(node));
        break;
      case ASTNode::STMT_FOR:
        visit_for_statement(*static_cast<ForStatement *>(node));
        break;
      case ASTNode::STMT_FOR_IN:
        visit_for_in_statement(*static_cast<ForInStatement *>(node));
        break;
      case ASTNode::STMT_SWITCH:
        visit_switch_statement(*static_cast<SwitchStatement *>(node));
        break;
      case ASTNode::EXPR_TRIPLE:
        visit_ternary_expr(*static_cast<TernaryExpr *>(node));
        break;
      case ASTNode::EXPR_REGEXP:
        visit_regexp(*static_cast<RegExpLiteral *>(node));
        break;
      case ASTNode::EXPR_COMMA:
        visit_comma_expr(*static_cast<Expression *>(node));
        break;
      case ASTNode::STMT_DO_WHILE:
        visit_do_while_statement(*static_cast<DoWhileStatement *>(node));
        break;
      case ASTNode::STMT_LABEL:
        visit_labelled_statement(*static_cast<LabelledStatement *>(node));
        break;
      case ASTNode::STMT_EMPTY:
        break;
      default:
        std::cout << node->description() << " not supported yet" << '\n';
        assert(false);
    }
  }

  void visit_single_statement(ASTNode *stmt) {
    if (not stmt->is(ASTNode::EXPR_ASSIGN)) {
      visit(stmt);
      // pop drop unused result
      if (stmt->is_expression() || stmt->is(ASTNode::FUNC)) {
        emit(OpType::pop_drop);
      }
    } else {
      visit_assignment_expr(*stmt->as<AssignmentExpr>(), false);
    }
  }

  void visit_program_or_function_body(ProgramOrFunctionBody& program) {
    scope().is_strict = program.strict;
    scope().associated_node = &program;

    // First, allocate space for the variables (defined by `let` and `const`) in this scope
    int deinit_begin = int(scope().get_var_next_index() + frame_meta_size);

    for (ASTNode *node : program.statements) {
      if (node->type != ASTNode::STMT_VAR) continue;
      auto& var_stmt = *static_cast<VarStatement *>(node);
      if (not var_stmt.is_lexical()) continue;

      for (VarDecl *decl : var_stmt.declarations) {
        bool res = scope().define_symbol(var_stmt.kind, decl->id.text);
        if (!res) {
          report_duplicated_id(to_u8string(decl->id.text), decl);
        }
      }
    }
    int deinit_end = int(scope().get_var_next_index() + frame_meta_size);
    // set local `let` and `const` variables to UNINIT.
    gen_var_deinit_code(deinit_begin, deinit_end);

    // begin codegen for inner functions
    codegen_inner_function(program.func_decls);

    std::vector<u32> top_level_throw;

    for (ASTNode *stmt : program.statements) {
      visit_single_statement(stmt);

      auto& throw_list = scope().throw_list;
      top_level_throw.insert(top_level_throw.end(), throw_list.begin(), throw_list.end());
      throw_list.clear();
    }

    if (program.type == ASTNode::PROGRAM) {
      emit(OpType::halt);
    } else { // function body
      emit(OpType::ret_undef);
    }

    assert(&scope() == scope().get_outer_func());
    scope().catch_table.emplace_back(0, 0, bytecode_pos());

    for (u32 idx : top_level_throw) {
      bytecode[idx].operand.two[0] = bytecode_pos();
    }

    if (program.type == ASTNode::PROGRAM) {
      emit(OpType::halt_err);
    } else {
      emit(OpType::ret_err);
    }
  }

  void gen_func_bytecode(Function& func, SmallVector<Instruction, 10>& init_code) {

    ProgramOrFunctionBody *body = func.body->as_func_body();
    u32 func_start_pos = bytecode_pos();
    push_scope(body->scope.get());
    // a function expression can call itself in its body using the function name.
    if (!func.is_stmt && func.has_name()) {
      auto res = scope().resolve_symbol(func.name.text);
      assert(not res.not_found());
      assert(res.storage_scope == ScopeType::FUNC);
      emit(OpType::store_curr_func, res.get_index());
    }

    // generate bytecode for function body
    visit_program_or_function_body(*body);

    // create metadata for function
    u32 func_idx = add_function_meta(JSFunctionMeta {
        .name_index = func.has_name() ? add_const(func.name.text) : 0,
        .is_anonymous = !func.has_name() || func.is_arrow_func,
        .is_arrow_func = func.is_arrow_func,
        .is_strict = body->strict,
        .param_count = (u16)scope().get_param_count(),
        .local_var_count = (u16)scope().get_var_count(),
        .stack_size = u16(scope().get_max_stack_size() + 1), // 1 more slot for error, maybe ?
        .bytecode_start = func_start_pos,
        .bytecode_end = bytecode_pos(),
        .source_line = func.start_line_num(),
        .catch_table = std::move(scope().catch_table)
    });

    // let the VM make a function
    init_code.emplace_back(OpType::make_func, func_idx);

    // capture closure variables
    // Only after visiting the body do we know which variables are captured
    for (auto symbol : scope().capture_list) {
      init_code.emplace_back(OpType::capture, scope_type_int(symbol.storage_scope),
                             symbol.get_index());
    }
    pop_scope();
  }

  void visit_comma_expr(Expression& expr) {
    for (ASTNode *ele : expr.elements) {
      assert(ele->type > ASTNode::BEGIN_EXPR && ele->type < ASTNode::END_EXPR);
      visit(ele);
      emit(OpType::pop_drop);
    }

    // don't pop the result of the last element. It's the result of the comma expression.
    bytecode.pop_back();
    scope().update_stack_usage(1);
  }

  void visit_function(Function& func) {
    auto& init_code = scope().inner_func_init_code[&func];
    for (auto& inst : init_code) {
      emit(inst);
    }
  }

  // note: `need_value` now only works for inc and dec.
  void visit_unary_expr(UnaryExpr& expr, bool need_value = true) {

    switch (expr.op.type) {
      case Token::ADD:
        visit(expr.operand);
        emit(OpType::js_to_number);
        break;
      case Token::LOGICAL_NOT:
        if (expr.is_prefix_op) {
          visit(expr.operand);
          emit(OpType::logi_not);
        } else {
          assert(false);
        }
        break;
      case Token::BIT_NOT:
        if (expr.is_prefix_op) {
          visit(expr.operand);
          emit(OpType::bits_not);
        } else {
          assert(false);
        }
        break;
      case Token::SUB:
        assert(expr.is_prefix_op);
        if (expr.operand->is(ASTNode::EXPR_NUMBER)) {
          emit(Instruction::num_imm(-expr.operand->as_number_literal()->num_val));
        } else {
          visit(expr.operand);
          emit(OpType::neg);
        }
        break;
      case Token::INC:
      case Token::DEC: {
        if (not expr.is_prefix_op && need_value) {
          visit(expr.operand);
          emit(OpType::js_to_number);
        }

        auto *num_1 = new NumberLiteral(1.0, u"1", 0, 0, 0);
        auto assign_type = expr.op.type == Token::INC ? Token::ADD_ASSIGN : Token::SUB_ASSIGN;
        auto *assign = new AssignmentExpr(assign_type, expr.operand, num_1, expr.get_source(),
                                          expr.start_pos(), expr.end_pos(), expr.start_line_num());
        // if it's a prefix op, we need the value produced by this assignment,
        // because prefix increment means "increase the value before get its value".
        // Otherwise, we need the old value instead of the value after assignment.
        visit_assignment_expr(*assign, expr.is_prefix_op && need_value);
        delete assign;              // will also delete `num_1`
        expr.operand = nullptr;     // avoid double free
        break;
      }
      case Token::KEYWORD:
        if (expr.op.text == u"typeof") {
          if (expr.operand->is(ASTNode::EXPR_ID)) {
            visit_identifier(*expr.operand, true);
          } else {
            visit(expr.operand);
          }
          emit(OpType::js_typeof);
        } else if (expr.op.text == u"delete") {
          if (auto *lhs = dynamic_cast<LeftHandSideExpr*>(expr.operand);
              lhs != nullptr && lhs->postfixs.size() != 0) {
            auto inst_set_prop = visit_left_hand_side_expr(*lhs, true, false);
            if (inst_set_prop.op_type == OpType::set_prop_atom) {
              emit(OpType::push_atom, inst_set_prop.operand.two[0]);
            }
            emit(OpType::js_delete);
          } else {
            assert(false);
          }
        } else if (expr.op.text == u"void") {
          visit_single_statement(expr.operand);
          emit(OpType::push_undef);
        } else {
          assert(false);
        }
        break;
      default:
        assert(false);
    }
  }

  void visit_binary_expr(BinaryExpr& expr) {

    if (expr.op.is_binary_logical()) {
      vector<U32Pair> true_list, false_list;
      visit_logical_expr(expr, true_list, false_list, true);

      for (auto& [idx, ord] : true_list) {
        bytecode[idx].operand.two[ord] = int(bytecode_pos());
      }
      for (auto& [idx, ord] : false_list) {
        bytecode[idx].operand.two[ord] = int(bytecode_pos());
      }
    }
    else {
      visit(expr.lhs);
      visit(expr.rhs);
      switch (expr.op.type) {
        case Token::ADD: emit(OpType::add); break;
        case Token::SUB: emit(OpType::sub); break;
        case Token::MUL: emit(OpType::mul); break;
        case Token::DIV: emit(OpType::div); break;

        case Token::MOD: emit(OpType::mod); break;

        case Token::BIT_AND: emit(OpType::bits_and); break;
        case Token::BIT_OR: emit(OpType::bits_or); break;
        case Token::BIT_XOR: emit(OpType::bits_xor); break;

        case Token::LSH: emit(OpType::lsh); break;
        case Token::RSH: emit(OpType::rsh); break;
        case Token::UNSIGNED_RSH: emit(OpType::ursh); break;

        case Token::NE: emit(OpType::ne); break;
        case Token::EQ: emit(OpType::eq); break;
        case Token::EQ3: emit(OpType::eq3); break;
        case Token::NE3: emit(OpType::ne3); break;

        case Token::LE: emit(OpType::le); break;
        case Token::LT: emit(OpType::lt); break;
        case Token::GT: emit(OpType::gt); break;
        case Token::GE: emit(OpType::ge); break;
        default: assert(false);
      }
    }

  }

  void visit_expr_in_logical_expr(ASTNode& expr, vector<U32Pair>& true_list,
                                  vector<U32Pair>& false_list, bool need_value) {
    if (expr.is_binary_logical_expr()) {
      visit_logical_expr(*expr.as_binary_expr(), true_list, false_list, need_value);
    }
    else if (expr.is_not_expr() && !need_value) {
      auto& not_expr = static_cast<UnaryExpr&>(expr);
      visit_expr_in_logical_expr(*not_expr.operand, true_list, false_list, need_value);

      vector<U32Pair> temp = std::move(false_list);
      false_list = std::move(true_list);
      true_list = std::move(temp);
    }
    else {
      visit(&expr);
      u32 jmp_pos = emit(need_value ? OpType::jmp_cond : OpType::jmp_cond_pop);
      true_list.push_back({jmp_pos, 0});
      false_list.push_back({jmp_pos, 1});
    }
  }

  void visit_logical_expr(BinaryExpr& expr, vector<U32Pair>& true_list, vector<U32Pair>& false_list,
                          bool need_value) {

    vector<U32Pair> lhs_true_list, lhs_false_list;
    visit_expr_in_logical_expr(*(expr.lhs), lhs_true_list, lhs_false_list, need_value);

    bool is_OR = expr.op.type == Token::LOGICAL_OR;
    if (is_OR) {
      // backpatch( B1.falselist, M.instr)
      for (auto& [idx, ord] : lhs_false_list) {
        bytecode[idx].operand.two[ord] = int(bytecode_pos());
      }
      true_list.insert(true_list.end(), lhs_true_list.begin(), lhs_true_list.end());
      if (need_value) emit(OpType::pop_drop);
    }
    else {
      for (auto& [idx, ord] : lhs_true_list) {
        bytecode[idx].operand.two[ord] = int(bytecode_pos());
      }
      false_list.insert(false_list.end(), lhs_false_list.begin(), lhs_false_list.end());
      if (need_value) emit(OpType::pop_drop);
    }

    vector<U32Pair> rhs_true_list, rhs_false_list;
    visit_expr_in_logical_expr(*(expr.rhs), rhs_true_list, rhs_false_list, need_value);
    // If the operator is OR, we take the rhs's false list as this expression's false list.
    if (is_OR) false_list = std::move(rhs_false_list);
    else true_list = std::move(rhs_true_list);

    // merge the true list.
    if (is_OR) true_list.insert(true_list.end(), rhs_true_list.begin(), rhs_true_list.end());
    else false_list.insert(false_list.end(), rhs_false_list.begin(), rhs_false_list.end());
  }

  void visit_assignment_expr(AssignmentExpr& expr, bool need_value = true) {
    auto assign_op = expr.assign_type;

    auto codegen_rhs = [&] () {
      if (assign_op != TokenType::ASSIGN) {
        if (expr.lhs->is_identifier()) {
          visit_identifier(*expr.lhs);
        } else {
          visit_left_hand_side_expr(*expr.lhs->as_lhs_expr(), false, false);
        }

        visit(expr.rhs);
        switch (assign_op) {
          case Token::ADD_ASSIGN:
          case Token::SUB_ASSIGN:
          case Token::MUL_ASSIGN:
          case Token::DIV_ASSIGN:
          case Token::MOD_ASSIGN: {
            auto diff = assign_op - Token::ADD_ASSIGN;
            auto op = static_cast<OpType>(static_cast<int>(OpType::add) + diff);
            emit(op);
            break;
          }
          case Token::LSH_ASSIGN:
            emit(OpType::lsh);
            break;
          case Token::RSH_ASSIGN:
            emit(OpType::rsh);
            break;
          case Token::UNSIGNED_RSH_ASSIGN:
            emit(OpType::ursh);
            break;
          case Token::AND_ASSIGN:
            emit(OpType::bits_and);
            break;
          case Token::OR_ASSIGN:
            emit(OpType::bits_or);
            break;
          case Token::XOR_ASSIGN:
            emit(OpType::bits_xor);
            break;
          case Token::LOGI_AND_ASSIGN:
          case Token::LOGI_OR_ASSIGN:
            // TODO
            assert(false);
          default:
            assert(false);
        }
      } else {
        visit(expr.rhs);
      }
    };

    if (expr.lhs_is_id()) {
      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());
      bool use_dynamic = lhs_sym.not_found()
                         || (lhs_sym.def_scope == ScopeType::GLOBAL && !lhs_sym.is_let_or_const());

      if (not use_dynamic) {
        if (expr.rhs_is_1()
            && (assign_op == TokenType::ADD_ASSIGN || assign_op == TokenType::SUB_ASSIGN)) {
          OpType op = assign_op == TokenType::ADD_ASSIGN  ? OpType::inc : OpType::dec;
          emit(op, scope_type_int(lhs_sym.storage_scope), lhs_sym.get_index());
          if (need_value) {
            emit(OpType::push, scope_type_int(lhs_sym.storage_scope), lhs_sym.get_index());
          }
          return;
        }

        codegen_rhs();
        OpType op;
        if (lhs_sym.is_let_or_const()) {
          op = need_value ? OpType::store_check : OpType::pop_check;
        } else {
          op = need_value ? OpType::store : OpType::pop;
        }
        emit(op, scope_type_int(lhs_sym.storage_scope), lhs_sym.get_index());
      } else {
        u32 atom = atom_pool.atomize(expr.lhs->get_source());
        emit(OpType::push_global_this);
        codegen_rhs();
        emit(OpType::set_prop_atom, int(atom));
        if (!need_value) emit(OpType::pop_drop);
      }
    }
    else {
      // check if left hand side is LeftHandSide Expression
      Instruction inst_set_prop;
      if (expr.lhs->type == ASTNode::EXPR_LHS) {
        inst_set_prop = visit_left_hand_side_expr(*expr.lhs->as_lhs_expr(), true, false);
      }
      if (inst_set_prop.op_type == OpType::nop) {
        assert(false);
      }
      codegen_rhs();
      emit(inst_set_prop);
      if (!need_value) emit(OpType::pop_drop);
    }
  }

  void visit_object_literal(ObjectLiteral& obj_lit) {
    emit(OpType::make_obj);
    for (auto& prop : obj_lit.properties) {
      // push the key into the stack
      emit(OpType::push_atom, (int)add_const(prop.key));
      // push the value into the stack
      visit(prop.value);
    }

    int prop_count = obj_lit.properties.size();
    if (prop_count > 0) {
      emit(OpType::add_props, prop_count);
      scope().update_stack_usage(-prop_count * 2);
    }
  }

  void visit_array_literal(ArrayLiteral& array_lit) {
    emit(OpType::make_array, (int)array_lit.len);

    for (auto& [idx, element] : array_lit.elements) {
      if (element) {
        visit(element);
      } else {
        emit(OpType::push_uninit);
      }
    }

    if (array_lit.len > 0) {
      emit(OpType::add_elements, (int)array_lit.len);
      scope().update_stack_usage(-(int)array_lit.len);
    }
  }

  Instruction visit_left_hand_side_expr(LeftHandSideExpr& expr, bool create_ref, bool in_new_ctx) {
    using LHS = LeftHandSideExpr::PostfixType;
    auto& postfixs = expr.postfixs;
    size_t postfix_size = postfixs.size();
    Instruction inst_set_prop;

    visit(expr.base);

#define CALL_AHEAD ((i + 1 < postfix_size) && postfixs[i+1].type == LHS::CALL)
#define NEW_CTX_LAST_CALL (in_new_ctx && (i + 1 == postfix_size - 1))

    for (size_t i = 0; i < postfix_size; i++) {
      auto postfix = postfixs[i];
      // obj.func()
      if (postfix.type == LHS::CALL) {
        visit_func_arguments(*postfix.subtree.args_expr);
        bool has_this = i > 0 && postfixs[i - 1].type != LHS::CALL;
        int arg_count = postfix.subtree.args_expr->arg_count();

        if (in_new_ctx && i == postfix_size - 1) [[unlikely]] {
          // if in new context, `visit_new_expr` will emit a js_new instruction.
        } else {
          emit(OpType::call, arg_count, int(has_this));
          scope().update_stack_usage(-arg_count);
        }
      }
      // obj.prop
      else if (postfix.type == LHS::PROP) {
        int key_id = (int)add_const(postfixs[i].subtree.prop_name);
        if (create_ref && i == postfix_size - 1) [[unlikely]] {
          inst_set_prop = Instruction(OpType::set_prop_atom, key_id);
        } else {
          if (CALL_AHEAD && !NEW_CTX_LAST_CALL) {
            emit(OpType::get_prop_atom2, key_id);
          } else {
            emit(OpType::get_prop_atom, key_id);
          }
        }
      }
      // obj[prop]
      else if (postfix.type == LHS::INDEX) {
        // evaluate the index expression
        assert(postfix.subtree.index_expr->is_expression());
        auto index_expr = postfix.subtree.index_expr;
        if (index_expr->is(ASTNode::EXPR_NUMBER)) [[likely]] {
          double num = index_expr->as_number_literal()->num_val;
          int64_t num_int = (int64_t)num;

          if (num >= 0 && double(num_int) == num && num_int < UINT32_MAX) {
            u32 atom = atom_pool.atomize_u32(num_int);
            emit(OpType::push_atom, atom);
          } else {
            emit(Instruction::num_imm(num));
          }
        } else if (index_expr->is(ASTNode::EXPR_STRING)) {
          auto& str = index_expr->as<StringLiteral>()->str_val;
          u32 atom = atom_pool.atomize(str);
          emit(OpType::push_atom, atom);
        } else {
          visit(postfix.subtree.index_expr);
        }

        if (create_ref && i == postfix_size - 1) [[unlikely]] {
          inst_set_prop = Instruction(OpType::set_prop_index);
        } else {
          if (CALL_AHEAD && !NEW_CTX_LAST_CALL) {
            emit(OpType::get_prop_index2);
          } else {
            emit(OpType::get_prop_index);
          }
        }
      }
    }

    return inst_set_prop;
  }

  void visit_identifier(ASTNode& id, bool no_throw = false) {
    auto symbol = scope().resolve_symbol(id.get_source());
    bool use_dynamic =
        symbol.not_found() || (symbol.def_scope == ScopeType::GLOBAL && !symbol.is_let_or_const());
    if (not use_dynamic) {
      if (symbol.is_let_or_const()) {
        emit(OpType::push_check, scope_type_int(symbol.storage_scope), symbol.get_index());
      } else {
        emit(OpType::push, scope_type_int(symbol.storage_scope), symbol.get_index());
      }
    } else {
      u32 atom = atom_pool.atomize(id.get_source());
      emit(no_throw ? OpType::dyn_get_var_undef : OpType::dyn_get_var, atom);
    }
  }

  void visit_func_arguments(Arguments& args) {
    for (auto node : args.args) {
      visit(node);
    }
  }

  void visit_number_literal(NumberLiteral& node) {
    emit(Instruction::num_imm(node.num_val));
  }

  void visit_string_literal(StringLiteral& node) {
    auto& str = node.str_val;
    emit(OpType::push_str, (int)add_const(str));
  }

  void visit_variable_statement(VarStatement& var_stmt, bool need_undef) {
    for (VarDecl *decl : var_stmt.declarations) {
      auto sym = scope().resolve_symbol(decl->id.text);
      if (need_undef && sym.is_let_or_const()) {
        emit(OpType::var_undef, int(sym.get_index()));
      }
      if (decl->var_init) {
        assert(decl->var_init->is(ASTNode::EXPR_ASSIGN));
        visit_assignment_expr(*decl->var_init->as<AssignmentExpr>(), false);
      }
    }
  }

  void visit_return_statement(ReturnStatement& return_stmt) {
    gen_call_finally_code(&scope(), scope().get_outer_func());

    if (return_stmt.expr) {
      visit(return_stmt.expr);
      emit(OpType::ret);
    } else {
      emit(OpType::ret_undef);
    }
  }

  void visit_if_statement(IfStatement& stmt) {
    vector<U32Pair> true_list, false_list;
    visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);
    for (auto& [idx, ord] : true_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    if (stmt.then_block->is_valid_in_single_stmt_ctx()) {
      visit_single_statement(stmt.then_block);
    } else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.then_block,
      });
    }

    u32 if_end_jmp = emit(OpType::jmp);

    for (auto& [idx, ord] : false_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    if (stmt.else_block) {
      if (stmt.else_block->is_valid_in_single_stmt_ctx()) {
        visit_single_statement(stmt.else_block);
      }
      else {
        report_error(CodegenError {
            .message =
                "SyntaxError: Lexical declaration cannot appear in a single-statement context",
            .ast_node = stmt.else_block,
        });
      }
    }
    bytecode[if_end_jmp].operand.two[0] = bytecode_pos();
  }

  void visit_ternary_expr(TernaryExpr& expr) {
    vector<U32Pair> true_list, false_list;
    visit_expr_in_logical_expr(*expr.cond_expr, true_list, false_list, false);

    for (auto& [idx, ord] : true_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    visit(expr.true_expr);
    u32 true_end_jmp = emit(OpType::jmp);

    for (auto& [idx, ord] : false_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    visit(expr.false_expr);
    bytecode[true_end_jmp].operand.two[0] = bytecode_pos();
  }

  void visit_regexp(RegExpLiteral& regexp) {
    auto& flags = regexp.flag;
    int pattern_atom = atom_pool.atomize(regexp.pattern);

    int re_flags = 0;
    for (char16_t flag : flags) {
      int mask;
      switch(flag) {
        case 'd':
          mask = LRE_FLAG_INDICES;
          break;
        case 'g':
          mask = LRE_FLAG_GLOBAL;
          break;
        case 'i':
          mask = LRE_FLAG_IGNORECASE;
          break;
        case 'm':
          mask = LRE_FLAG_MULTILINE;
          break;
        case 's':
          mask = LRE_FLAG_DOTALL;
          break;
        case 'u':
          mask = LRE_FLAG_UNICODE;
          break;
        case 'y':
          mask = LRE_FLAG_STICKY;
          break;
        default:
          goto bad_flags;
      }
      if ((re_flags & mask) != 0) {
      bad_flags:
        report_error(CodegenError {
            .type = JS_SYNTAX_ERROR,
            .message = "Invalid regular expression flags",
            .ast_node = &regexp,
        });
        return;
      }
      re_flags |= mask;
    }

    emit(OpType::regexp_build, pattern_atom, re_flags);
  }

  void visit_while_statement(WhileStatement& stmt) {
    u32 loop_start = bytecode_pos();
    scope().continue_pos = loop_start;
    scope().can_break = true;
    scope().can_continue = true;

    vector<U32Pair> true_list, false_list;
    visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);

    for (auto& [idx, ord] : true_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    bool body_is_block = false;
    if (stmt.body_stmt->is_block()) {
      body_is_block = true;
      stmt.body_stmt->as_block()->scope->associated_node = &stmt;
      stmt.body_stmt->as_block()->scope->set_block_type(BlockType::WHILE);
      visit_block_statement(*stmt.body_stmt->as<Block>(), false, _, _, _);
    }
    else if (stmt.body_stmt->is_valid_in_single_stmt_ctx()) {
      visit(stmt.body_stmt);
    }
    else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.body_stmt,
      });
    }
    emit(OpType::jmp, loop_start);

    for (auto& [idx, ord] : false_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    for (u32 idx : scope().break_list) {
      bytecode[idx].operand.two[0] = bytecode_pos();
    }
    scope().break_list.clear();
    // dispose the variables in the loop body
    if (body_is_block) {
      gen_var_dispose_code(*stmt.body_stmt->as<Block>()->scope);
    }

    scope().can_break = false;
    scope().can_continue = false;
    scope().continue_pos = -1;
  }

  void visit_do_while_statement(DoWhileStatement& stmt) {
    scope().can_continue = true;
    scope().can_break = true;

    u32 loop_start = bytecode_pos();

    if (stmt.body_stmt->is_block()) [[likely]] {
      stmt.body_stmt->as_block()->scope->associated_node = &stmt;
      stmt.body_stmt->as_block()->scope->set_block_type(BlockType::DO_WHILE);
      visit_block_statement(*stmt.body_stmt->as_block(), false, _, _, _);
    }
    else if (stmt.body_stmt->is_valid_in_single_stmt_ctx()) {
      visit(stmt.body_stmt);
    }
    else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.body_stmt,
      });
    }

    for (u32 idx : scope().continue_list) {
      bytecode[idx].operand.two[0] = bytecode_pos();
    }
    scope().continue_list.clear();

    vector<U32Pair> true_list, false_list;
    visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);

    for (auto& [idx, ord] : true_list) {
      bytecode[idx].operand.two[ord] = loop_start;
    }
    for (auto& [idx, ord] : false_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    for (u32 idx : scope().break_list) {
      bytecode[idx].operand.two[0] = bytecode_pos();
    }
    scope().break_list.clear();
    // dispose the variables in the loop body
    gen_var_dispose_code(*stmt.body_stmt->as<Block>()->scope);
    scope().can_break = false;
    scope().can_continue = false;
  }

  void visit_for_statement(ForStatement& stmt) {
    Scope& outer_scope = scope();
    outer_scope.can_break = true;
    outer_scope.can_continue = true;
    outer_scope.continue_pos = -1;

    vector<pair<u16string_view, VarKind>> extra_var;
    pair<u32, u32> extra_var_range {0, 0};

    auto init = [&, this] () {
      auto init_expr = stmt.init_expr;
      if (!init_expr) return;

      if (init_expr->is(ASTNode::STMT_VAR)
          && init_expr->as<VarStatement>()->is_lexical()) {

        auto *var_stmt = init_expr->as<VarStatement>();
        for (auto *decl : var_stmt->declarations) {
          extra_var.emplace_back(decl->id.text, var_stmt->kind);
          scope().define_symbol(var_stmt->kind, decl->id.text);
        }
        extra_var_range = scope().get_var_index_range(frame_meta_size);
      }

      if (init_expr->is(ASTNode::STMT_VAR)) {
        // TODO: may want to set the last arg to `true` to ensure that we always
        // have undefined value for new variables. Now `false` is fine because we always
        // clean the local variables when leaving a scope
        visit_variable_statement(*init_expr->as<VarStatement>(), false);
      } else {
        visit_single_statement(init_expr);
      }
    };

    vector<U32Pair> true_list, false_list;
    u32 loop_start_pos;

    auto prolog = [&, this] {
      auto init_expr = stmt.init_expr;
      if (init_expr) {
        if (init_expr->is(ASTNode::STMT_VAR)) {
          visit_variable_statement(*init_expr->as<VarStatement>(), false);
        } else {
          visit_single_statement(init_expr);
        }
      }

      loop_start_pos = bytecode_pos();
      if (!stmt.condition_expr) return;
      visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);
      for (auto& [idx, ord] : true_list) {
        bytecode[idx].operand.two[ord] = bytecode_pos();
      }
    };

    auto epilog = [&, this] {
      for (u32 idx : outer_scope.continue_list) {
        bytecode[idx].operand.two[0] = bytecode_pos();
      }
      outer_scope.continue_list.clear();

      for (auto [var_name, kind] : extra_var) {
        auto sym = scope().resolve_symbol(var_name);
        if (sym.original_symbol->is_captured) {
          emit(OpType::loop_var_renew, sym.get_index());
        }
      }
      auto *inc = stmt.increment_expr;
      if (inc) {
        if (inc->is(ASTNode::EXPR_ASSIGN)) {
          visit_assignment_expr(*inc->as<AssignmentExpr>(), false);
        } else if (inc->is(ASTNode::EXPR_UNARY) && inc->as<UnaryExpr>()->is_inc_or_dec()) {
          visit_unary_expr(*inc->as<UnaryExpr>(), false);
        } else {
          // no need to use `visit_single_statement` here
          visit(inc);
          emit(OpType::pop_drop);
        }
      }
      emit(OpType::jmp, (int)loop_start_pos);
    };

    bool body_is_block = false;
    if (stmt.body_stmt->is_valid_in_single_stmt_ctx()) {
      if (stmt.body_stmt->is_block()) {
        body_is_block = true;
        stmt.body_stmt->as_block()->scope->associated_node = &stmt;
        stmt.body_stmt->as_block()->scope->set_block_type(BlockType::FOR);
        visit_block_statement(*stmt.body_stmt->as<Block>(), false, init, prolog, epilog);
      } else {
        Scope fake_scope(ScopeType::BLOCK, &scope());
        fake_scope.associated_node = &stmt;
        fake_scope.set_block_type(BlockType::FOR);
        push_scope(&fake_scope);
        init();
        prolog();
        visit_single_statement(stmt.body_stmt);
        epilog();
        pop_scope();
      }
    } else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.body_stmt,
      });
    }

    for (auto& [idx, ord] : false_list) {
      bytecode[idx].operand.two[ord] = bytecode_pos();
    }

    for (u32 idx : outer_scope.break_list) {
      bytecode[idx].operand.two[0] = bytecode_pos();
    }
    outer_scope.break_list.clear();

    if (body_is_block) {
      gen_var_dispose_code(*stmt.body_stmt->as<Block>()->scope);
    } else {
      gen_var_dispose_code(extra_var_range.first, extra_var_range.second);
    }

    outer_scope.can_break = false;
    outer_scope.can_continue = false;
    outer_scope.continue_pos = -1;
  }

  void visit_for_in_statement(ForInStatement& stmt) {
    auto for_in = stmt.iter_type == ForInStatement::FOR_IN;
    Scope& outer_scope = scope();
    outer_scope.can_break = true;
    outer_scope.can_continue = true;
    outer_scope.continue_pos = -1;

    visit(stmt.collection_expr);
    for_in ? emit(OpType::for_in_init) : emit(OpType::for_of_init);

    optional<pair<u16string_view, VarKind>> extra_var;
    u32 extra_var_index;
    auto init = [&, this] {
      auto ele_expr = stmt.element_expr;
      if (not ele_expr->is(ASTNode::STMT_VAR)) [[unlikely]] return;

      if (ele_expr->as<VarStatement>()->is_lexical()) {
        auto *var_stmt = ele_expr->as<VarStatement>();
        assert(var_stmt->declarations.size() == 1);
        VarDecl *decl = var_stmt->declarations[0];

        extra_var = {decl->id.text, var_stmt->kind};
        scope().define_symbol(var_stmt->kind, decl->id.text);
        extra_var_index = scope().get_var_start_index();
      }
    };

    u32 loop_start;
    u32 exit_jump;
    auto prolog = [&, this] {
      loop_start = for_in ? emit(OpType::for_in_next) : emit(OpType::for_of_next);
      exit_jump = emit(OpType::iter_end_jmp);
      scope().update_stack_usage(-1);

      if (stmt.element_is_id()) {
        auto id = stmt.get_element_id();
        auto sym = scope().resolve_symbol(id);
        bool dynamic = (sym.def_scope == ScopeType::GLOBAL && !sym.is_let_or_const())
                       || sym.not_found();

        if (not dynamic) {
          OpType op = sym.is_let_or_const() ? OpType::pop_check : OpType::pop;
          emit(op, scope_type_int(sym.storage_scope), sym.get_index());
        } else {
          u32 atom = atom_pool.atomize(id);
          emit(OpType::dyn_set_var, int(atom));
          emit(OpType::pop_drop);
        }
      }
      else { // LeftHandSide Expression
        Instruction inst_set_prop;
        if (stmt.element_expr->type == ASTNode::EXPR_LHS) {
          inst_set_prop = visit_left_hand_side_expr(*stmt.element_expr->as_lhs_expr(), true, false);
        }

        if (inst_set_prop.op_type == OpType::set_prop_atom) {
          emit(OpType::move_to_top1);
        } else if (inst_set_prop.op_type == OpType::set_prop_index) {
          emit(OpType::move_to_top2);
        } else {
          assert(false);
        }
        emit(inst_set_prop);
        emit(OpType::pop_drop);
      }
    };

    auto epilog = [&, this] {
      for (u32 idx : outer_scope.continue_list) {
        bytecode[idx].operand.two[0] = bytecode_pos();
      }
      outer_scope.continue_list.clear();

      if (extra_var.has_value()) {
        auto sym = scope().resolve_symbol(extra_var->first);
        if (sym.original_symbol->is_captured) {
          emit(OpType::loop_var_renew, sym.get_index());
        }
      }
      emit(OpType::jmp, (int)loop_start);
    };

    bool body_is_block = false;
    if (stmt.body_stmt->is_valid_in_single_stmt_ctx()) {
      if (stmt.body_stmt->is_block()) {
        body_is_block = true;
        stmt.body_stmt->as_block()->scope->associated_node = &stmt;
        stmt.body_stmt->as_block()->scope->set_block_type(BlockType::FOR_IN);
        visit_block_statement(*stmt.body_stmt->as<Block>(), false, init, prolog, epilog);
      } else {
        Scope fake_scope(ScopeType::BLOCK, &scope());
        fake_scope.associated_node = &stmt;
        fake_scope.set_block_type(BlockType::FOR_IN);
        push_scope(&fake_scope);
        init();
        prolog();
        visit_single_statement(stmt.body_stmt);
        epilog();
        pop_scope();
      }
    } else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.body_stmt,
      });
    }
    u32 loop_end = bytecode_pos();
    bytecode[exit_jump].operand.two[0] = loop_end;
    for (u32 idx : outer_scope.break_list) {
      bytecode[idx].operand.two[0] = loop_end;
    }
    outer_scope.break_list.clear();

    if (body_is_block) {
      gen_var_dispose_code(*stmt.body_stmt->as<Block>()->scope);
    } else if (extra_var.has_value()) {
      emit(OpType::var_dispose, extra_var_index);
    }
    // drop the ForInIterator
    emit(OpType::pop_drop);

    outer_scope.can_break = false;
    outer_scope.can_continue = false;
  }

  void visit_switch_statement(SwitchStatement& stmt) {
    scope().can_break = true;
    push_scope(stmt.scope.get());
    scope().associated_node = &stmt;
    scope().set_block_type(BlockType::SWITCH);

    u32 deinit_begin = scope().get_var_next_index();
    for (VarStatement *var_stmt : stmt.lexical_var_def) {
      for (VarDecl *decl : var_stmt->declarations) {
        bool res = scope().define_symbol(var_stmt->kind, decl->id.text);
        if (!res) {
          report_error(CodegenError {
              .message = "Identifier " + decl->id.get_text_utf8()  + " has already been declared",
              .ast_node = decl,
          });
        }
      }
    }
    u32 deinit_end = scope().get_var_next_index();
    gen_var_deinit_code(deinit_begin, deinit_end);

    codegen_inner_function(stmt.func_decls);

    visit(stmt.condition_expr);
    // code gen the cases comparison
    for (auto& case_clause : stmt.cases) {
      emit(OpType::dup_stack_top);
      visit(case_clause.expr);
      emit(OpType::eq3);
      case_clause.jump_point = emit(OpType::jmp_true_pop);
    }
    u32 jmp_to_default = emit(OpType::jmp);

    auto visit_case_stmts = [this] (SwitchStatement::CaseClause& case_clause) {
      bytecode[case_clause.jump_point].operand.two[0] = bytecode_pos();
      for (auto stmt : case_clause.stmts) {
        if (stmt->is(ASTNode::FUNC) && stmt->as_function()->is_stmt) continue;
        visit_single_statement(stmt);
      }
    };

    u32 default_idx = stmt.has_default ? stmt.default_index : stmt.cases.size();
    for (size_t i = 0; i < default_idx; i++) {
      visit_case_stmts(stmt.cases[i]);
    }

    if (stmt.has_default) {
      bytecode[jmp_to_default].operand.two[0] = bytecode_pos();
      for (auto s : stmt.default_stmts) {
        if (s->is(ASTNode::FUNC) && s->as_function()->is_stmt) continue;
        visit_single_statement(s);
      }
      // cases after the `default`
      for (size_t i = default_idx; i < stmt.cases.size(); i++) {
        visit_case_stmts(stmt.cases[i]);
      }
    }

    if (not stmt.has_default) {
      bytecode[jmp_to_default].operand.two[0] = bytecode_pos();
    }
    for (u32 idx : scope().get_outer()->break_list) {
      bytecode[idx].operand.two[0] = bytecode_pos();
    }
    scope().get_outer()->break_list.clear();
    // drop the condition value
    emit(OpType::pop_drop);

    pop_scope();
    scope().can_break = false;
  }

  void visit_labelled_statement(LabelledStatement& stmt) {
    visit(stmt.statement);
  }

  void gen_call_finally_code(Scope *curr, Scope *end) {
    while (curr != end) {
      assert(curr->get_type() == ScopeType::BLOCK);
      auto block_type = curr->get_block_type();
      auto *outer = curr->get_outer();
      if (block_type == BlockType::TRY_FINALLY || block_type == BlockType::CATCH_FINALLY) {
        outer->call_procedure_list.push_back(emit_keep_stack(OpType::proc_call));
      }
      curr = outer;
    }
  }

  void visit_continue_break_statement(ContinueOrBreak& stmt) {

    auto resolve_label = [] (Scope *scope, u16string_view label) {
      Scope::ControlRedirectResult res;
      while (scope->get_type() == ScopeType::BLOCK) {
        auto node = scope->associated_node;
        assert(node);
        if (node->label == label) {
          if (node->is_loop() || node->is_block()) {
            res.found = true;
            res.target_scope = scope;
          }
          break;
        }
        // these two kind of statements put 1 value on the operand stack,
        // which is the iterator object.
        if (node->is(ASTNode::STMT_FOR_IN)
            || node->is(ASTNode::STMT_SWITCH)
            || scope->get_block_type() == BlockType::FINALLY_NORM) {
          res.stack_drop_cnt += 1;
        }
        scope = scope->get_outer();
      }
      return res;
    };

    auto resolve_break_usual = [] (Scope *scope) {
      Scope::ControlRedirectResult res;
      bool meet_loop_or_switch = false;
      while (true) {
        if (scope->can_break && meet_loop_or_switch) {
          res.found = true;
          res.target_scope = scope;
          return res;
        } else if (scope->get_type() == ScopeType::BLOCK) {
          meet_loop_or_switch |= scope->associated_node->is_loop();
          meet_loop_or_switch |= scope->associated_node->is(ASTNode::STMT_SWITCH);
          if (scope->get_block_type() == BlockType::FINALLY_NORM) {
            res.stack_drop_cnt += 1;
          }
          scope = scope->get_outer();
          assert(scope);
        } else {
          return res;
        }
      }
    };

    auto resolve_continue_usual = [] (Scope *scope) {
      Scope::ControlRedirectResult res;
      auto prev_node_type = ASTNode::ILLEGAL;
      while (true) {
        if (scope->can_continue) {
          res.found = true;
          res.target_scope = scope;
          // no need to drop if we are going to continue this `for-in` loop
          if (prev_node_type == ASTNode::STMT_FOR_IN) {
            res.stack_drop_cnt -= 1;
          }
          return res;
        }
        else if (scope->get_type() == ScopeType::BLOCK) {
          auto node = scope->associated_node;
          assert(node);
          if (node->is(ASTNode::STMT_FOR_IN)
              || node->is(ASTNode::STMT_SWITCH)
              || scope->get_block_type() == BlockType::FINALLY_NORM) {
            res.stack_drop_cnt += 1;
          }
          prev_node_type = node->type;
          scope = scope->get_outer();
          assert(scope);
        }
        else {
          return res;
        }
      }
    };


    if (stmt.type == ASTNode::STMT_CONTINUE) {
      Scope *continue_scope;
      if (stmt.id.is(TokenType::NONE)) [[likely]] {
        auto resolve_res = resolve_continue_usual(&scope());
        assert(resolve_res.found);
        continue_scope = resolve_res.target_scope;

        gen_var_dispose_code_recursive(&scope(), [] (Scope *s) {
          return !s->is_continue_target && !s->can_continue;
        });
        for (int i = 0; i < resolve_res.stack_drop_cnt; i++) {
          emit_keep_stack(OpType::pop_drop);
        }
      }
      // continue to a label
      else {
        auto resolve_res = resolve_label(&scope(), stmt.id.text);
        if (resolve_res.found) [[likely]] {
          // continue to a block is not allowed
          if (resolve_res.target_scope->associated_node->is_block()) [[unlikely]] {
            report_error(CodegenError {
                .message = "Illegal continue statement: '" + to_u8string(stmt.id.text)
                           + "' does not denote an iteration statement",
                .ast_node = &stmt,
            });
            return;
          }
          continue_scope = resolve_res.target_scope->get_outer();

          gen_var_dispose_code_recursive(&scope(), [&] (Scope *s) {
            return !(s == resolve_res.target_scope);
          });
          for (int i = 0; i < resolve_res.stack_drop_cnt; i++) {
            emit_keep_stack(OpType::pop_drop);
          }
        } else {
          report_error(CodegenError {
              .message = "Undefined label " + to_u8string(stmt.id.text),
              .ast_node = &stmt,
          });
          return;
        }
      }
      assert(continue_scope);
      gen_call_finally_code(&scope(), continue_scope);
      if (continue_scope->continue_pos != -1) {
        emit(OpType::jmp, u32(continue_scope->continue_pos));
      } else {
        continue_scope->continue_list.push_back(emit(OpType::jmp));
      }
    }
    // break
    else {
      Scope *break_scope;
      if (stmt.id.is(TokenType::NONE)) [[likely]] {
        gen_var_dispose_code_recursive(&scope(), [] (Scope *s) {
          return !s->is_break_target && !s->can_break;
        });
        break_scope = resolve_break_usual(&scope()).target_scope;
      }
      // break to a label
      else {
        auto resolve_res = resolve_label(&scope(), stmt.id.text);
        if (resolve_res.found) [[likely]] {
          gen_var_dispose_code_recursive(&scope(), [&] (Scope *s) {
            return !(s == resolve_res.target_scope);
          });
          for (int i = 0; i < resolve_res.stack_drop_cnt; i++) {
            emit_keep_stack(OpType::pop_drop);
          }
          break_scope = resolve_res.target_scope->get_outer();
        } else {
          report_error(CodegenError {
              .message = "Undefined label " + to_u8string(stmt.id.text),
              .ast_node = &stmt,
          });
          return;
        }
      }
      assert(break_scope);
      gen_call_finally_code(&scope(), break_scope);
      break_scope->break_list.push_back(emit(OpType::jmp));
    }
  }

  void codegen_inner_function(const vector<Function *>& stmts) {
    if (scope().inner_func_init_code.empty()) return;

    u32 jmp_inst_idx = emit(OpType::jmp);

    // generate bytecode for functions first, then record its function meta index in the map.
    for (Function *func : scope().inner_func_order) {
      gen_func_bytecode(*func, scope().inner_func_init_code[func]);
    }
    // skip function bytecode
    bytecode[jmp_inst_idx].operand.two[0] = bytecode_pos();

    auto env_scope_type = scope().get_outer_func()->get_type();
    for (Function *node : stmts) {
      auto& func = *node;
      assert(node->type == ASTNode::FUNC);
      assert(func.has_name());
      assert(func.is_stmt);

      if (env_scope_type == ScopeType::GLOBAL) {
        emit(OpType::push_global_this);
      }
      visit_function(func);

      auto symbol = scope().resolve_symbol(func.name.text);
      if (env_scope_type == ScopeType::GLOBAL) {
        emit(OpType::set_prop_atom, atom_pool.atomize(func.name.text));
        emit(OpType::pop_drop);
      } else {
        assert(!symbol.not_found());
        emit(OpType::pop, scope_type_int(symbol.storage_scope), symbol.get_index());
      }
    }
  }

  // If `extra_var` is not empty, it means that the external wants to define some variables
  // in advance in this block.
  template <typename F1, typename F2, typename F3>
  requires std::invocable<F1> && std::invocable<F2> && std::invocable<F3>
  void visit_block_statement(Block& block, bool dispose_var,
                             const F1& init, const F2& prolog, const F3& epilog) {

    push_scope(block.scope.get());
    init();

    // as for now, the only usage of extra vars is for the catch variables. and they don't need
    // to be deinited.
    u32 deinit_begin = scope().get_var_next_index();

    for (ASTNode *node : block.statements) {
      if (node->type != ASTNode::STMT_VAR) continue;
      auto& var_stmt = *node->as<VarStatement>();
      if (not var_stmt.is_lexical()) continue;

      for (VarDecl *decl : var_stmt.declarations) {
        bool res = scope().define_symbol(var_stmt.kind, decl->id.text);
        if (!res) {
          report_duplicated_id(decl->id.get_text_utf8(), decl);
        }
      }
    }
    u32 deinit_end = scope().get_var_next_index();

    prolog();
    gen_var_deinit_code(deinit_begin, deinit_end);

    // begin codegen for inner functions
    codegen_inner_function(block.func_decls);

    for (auto *stmt : block.statements) {
      visit_single_statement(stmt);
    }

    epilog();

    if (not block.label.empty()) {
      for (u32 idx : scope().get_outer()->break_list) {
        bytecode[idx].operand.two[0] = bytecode_pos();
      }
      scope().get_outer()->break_list.clear();
    }
    // dispose the scope variables after leaving the block scope
    if (dispose_var) gen_var_dispose_code(scope());
    pop_scope();
  }

  template <typename Func>
  requires std::invocable<Func, Scope*> && std::same_as<std::invoke_result_t<Func, Scope*>, bool>
  void gen_var_dispose_code_recursive(Scope *scope, Func predicate) {
    while (true) {
      auto scope_type = scope->get_type();
      bool should_clean = scope_type != ScopeType::GLOBAL
                          && scope_type != ScopeType::FUNC
                          && predicate(scope);
      if (!should_clean) break;

      gen_var_dispose_code(*scope);
      scope = scope->get_outer();
    }
  }

  void gen_var_dispose_code(Scope& scope) {
    auto [disp_begin, disp_end] = scope.get_var_index_range(frame_meta_size);
    gen_var_dispose_code(disp_begin, disp_end);
  }

  void gen_var_dispose_code(u32 begin, u32 end) {
    if (end - begin == 1) {
      emit(OpType::var_dispose, begin);
    } else if (end - begin > 1) {
      emit(OpType::var_dispose_range, begin, end);
    }
  }

  void gen_var_deinit_code(u32 begin, u32 end) {
    if (end - begin == 1) {
      emit(OpType::var_deinit, begin);
    } else if (end - begin > 1) {
      emit(OpType::var_deinit_range, begin, end);
    }
  }

  void visit_try_statement(TryStatement& stmt) {
    assert(stmt.try_block->type == ASTNode::STMT_BLOCK);

    bool has_catch = stmt.catch_block != nullptr;
    bool has_finally = stmt.finally_block != nullptr;
    scope().has_try = true;
    u32 try_start = bytecode_pos();
    // reserve a stack element for exception
    scope().update_stack_usage(1);

    Block *try_blk = stmt.try_block->as_block();
    try_blk->scope->associated_node = &stmt;
    try_blk->scope->set_block_type(has_finally ? BlockType::TRY_FINALLY : BlockType::TRY);
    visit_block_statement(*try_blk, true, _, _, _);

    u32 try_end = bytecode_pos() - 1;
    scope().has_try = false;

    SmallVector<u32, 2> proc_call_inst;
    if (has_finally) {
      // this will call the `finally2`.
      proc_call_inst.push_back(emit(OpType::proc_call));
    }
    u32 try_end_jmp = emit(OpType::jmp);
    u32 catch_or_finally_pos = try_end_jmp + 1;

    // `throw` statements will jump here
    for (u32 idx : scope().throw_list) {
      bytecode[idx].operand.two[0] = int(catch_or_finally_pos);
    }
    scope().throw_list.clear();
    // also, any error in the try block will jump here. So we should add a catch table entry.
    auto& catch_table = scope().get_outer_func()->catch_table;
    catch_table.emplace_back(try_start, try_end, catch_or_finally_pos);
    // We are going to record the address of the variables in the try block.
    // When an exception happens, we should dispose the variables in this block.
    pair<u32, u32> var_range = try_blk->scope->get_var_index_range(frame_meta_size);
    catch_table.back().local_var_begin = var_range.first;
    catch_table.back().local_var_end = var_range.second;

    // in the case of `catch (a) ...`, we are going to store the top-of-stack value to a local
    // variable. The variable is defined as `let` inside the catch block. And since it is the
    // first variable in the catch block, we know it must be at `var_next_index`.
    if (has_catch) {
      if (stmt.catch_ident.is(TokenType::IDENTIFIER)) {
        int catch_id_addr = scope().get_var_next_index() + frame_meta_size;
        emit(OpType::pop, scope_type_int(scope().get_outer_func()->get_type()), catch_id_addr);
      } else {
        emit(OpType::pop_drop);
      }
    }

    u32 catch_end_jmp;
    if (has_catch) {
      u32 catch_start = bytecode_pos();

      auto init = [&, this] () {
        if (stmt.catch_ident.is(TokenType::IDENTIFIER)) {
          scope().define_symbol(VarKind::DECL_LET, stmt.catch_ident.text);
        }
      };
      Block *catch_blk = stmt.catch_block->as_block();
      catch_blk->scope->associated_node = &stmt;
      catch_blk->scope->set_block_type(has_finally ? BlockType::CATCH_FINALLY : BlockType::CATCH);
      if (has_finally) scope().has_try = true;
      visit_block_statement(*catch_blk, true, init, _, _);
      if (has_finally) scope().has_try = false;

      u32 catch_end = bytecode_pos() - 1;

      if (has_finally) {
        // this will call the `finally2`.
        proc_call_inst.push_back(emit_keep_stack(OpType::proc_call));
        catch_end_jmp = emit(OpType::jmp);
        // the `throw` in the `catch` block should go here
        u32 finally1_start = bytecode_pos();

        for (u32 idx : scope().throw_list) {
          bytecode[idx].operand.two[0] = int(finally1_start);
        }
        scope().throw_list.clear();

        catch_table.emplace_back(catch_start, catch_end, finally1_start);
        var_range = catch_blk->scope->get_var_index_range(frame_meta_size);
        catch_table.back().local_var_begin = var_range.first;
        catch_table.back().local_var_end = var_range.second;
      }
    }

    if (has_finally) {
      // finally1. If there is error in the try block (without catch) or in the catch block, go here.
      Block *finally_blk = stmt.finally_block->as_block();
      finally_blk->scope->associated_node = &stmt;
      finally_blk->scope->set_block_type(BlockType::FINALLY_ECPT);
      visit_block_statement(*finally_blk, true, _, _, _);
      scope().resolve_throw_list()->push_back(emit(OpType::jmp));

      u32 finally2_start = bytecode_pos();
      for (u32 idx : proc_call_inst) {
        bytecode[idx].operand.two[0] = finally2_start;
      }
      for (u32 idx : scope().call_procedure_list) {
        bytecode[idx].operand.two[0] = finally2_start;
      }
      scope().call_procedure_list.clear();
      // finally2
      finally_blk->scope->set_block_type(BlockType::FINALLY_NORM);
      visit_block_statement(*finally_blk, true, _, _, _);
      emit(OpType::proc_ret);
    }

    bytecode[try_end_jmp].operand.two[0] = bytecode_pos();
    if (has_catch && has_finally) {
      bytecode[catch_end_jmp].operand.two[0] = bytecode_pos();
    }
  }

  void visit_throw_statement(ThrowStatement& stmt) {
    assert(stmt.expr->is_expression());
    visit(stmt.expr);

    gen_var_dispose_code_recursive(&scope(), [] (Scope *s) {
      return !s->has_try;
    });

    scope().resolve_throw_list()->push_back(bytecode_pos());
    emit(OpType::jmp);
  }

  void visit_new_expr(NewExpr& expr) {
    if (expr.callee->is_identifier()) {
      visit_identifier(*expr.callee);
      scope().update_stack_usage(1);
      emit(OpType::js_new, 0);
      scope().update_stack_usage(-1);
    }
    else if (expr.callee->is_lhs_expr()) {
      auto& lhs_expr = *expr.callee->as_lhs_expr();
      visit_left_hand_side_expr(lhs_expr, false, true);
      int arg_count = 0;
      if (lhs_expr.postfixs.back().type == LeftHandSideExpr::CALL) {
        arg_count = lhs_expr.postfixs.back().subtree.args_expr->arg_count();
      }
      scope().update_stack_usage(1);
      emit(OpType::js_new, arg_count);
      scope().update_stack_usage(-arg_count - 1);
    }
    else {
      assert(false);
    }
  }

  const SmallVector<CodegenError, 10>& get_errors() { return errors; }

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
      scope->get_outer_func()->update_var_count(scope->get_var_next_index());
      scope->get_outer_func()->update_max_stack_size(scope->get_max_stack_size());
    }
  }

  template <typename... Args>
  u32 emit(OpType inst_type, Args&&...args) {
    update_stack_usage_common(inst_type);
    bytecode.emplace_back(inst_type, std::forward<Args>(args)...);
    return bytecode.size() - 1;
  }

  template <typename... Args>
  u32 emit_keep_stack(OpType inst_type, Args&&...args) {
    bytecode.emplace_back(inst_type, std::forward<Args>(args)...);
    return bytecode.size() - 1;
  }

  u32 emit(Instruction inst) {
    update_stack_usage_common(inst.op_type);
    bytecode.push_back(inst);
    return bytecode.size() - 1;
  }

  void update_stack_usage_common(OpType inst_type) {
    int diff = Instruction::get_stack_usage(inst_type);
    scope().update_stack_usage(diff);
  }

  void report_error(CodegenError err) {
    err.describe();
    errors.push_back(std::move(err));
  }

  void report_duplicated_id(string id, ASTNode *node) {
    report_error(CodegenError {
        .message = "Identifier " + id  + " has already been declared",
        .ast_node = node,
    });
  }

  u32 add_const(u16string_view str_view) {
    auto idx = atom_pool.atomize(str_view);
    return idx;
  }

  u32 add_const(double num) {
    auto idx = num_list.size();
    num_list.push_back(num);
    return idx;
  }

  u32 add_function_meta(const JSFunctionMeta& meta) {
    auto idx = func_meta.size();
    func_meta.push_back(meta);
    return idx;
  }

  u32 bytecode_pos() { return bytecode.size(); }

  std::vector<Scope *> scope_chain;
  std::vector<Instruction> bytecode;
  SmallVector<CodegenError, 10> errors;

  // for constant
  AtomPool atom_pool;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  inline static auto _ = [] {};
};

} // namespace njs

#endif // NJS_CODEGEN_VISITOR_H