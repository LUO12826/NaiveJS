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

  void describe() {
    printf("At line %u: %s\n", ast_node->get_line_start(), message.c_str());
  }
};

class CodegenVisitor {
  friend class NjsVM;

 public:
  unordered_map<u16string, u32> global_props_map;

  void codegen(ProgramOrFunctionBody *prog) {

    push_scope(prog->scope.get());
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
      std::cout << std::setw(3) << i << " " << to_u8string(str_list[i]) << '\n';
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
                                                : to_u8string(str_list[meta.name_index]);
      std::cout << "index: " << std::setw(3) << i
                << " name: " << std::setw(30) << func_name
                << " addr: " << meta.code_address << '\n';

      for (auto& catch_item : meta.catch_table) {
        std::cout << "  " << catch_item.description() << '\n';
      }
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

    for (auto& entry : scope_chain[0]->catch_table) {
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
        visit_left_hand_side_expr(*static_cast<LeftHandSideExpr *>(node), false, false);
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
      case ASTNode::AST_EXPR_THIS: {
        bool in_global = scope().get_outer_func()->get_scope_type() == ScopeType::GLOBAL;
        emit(InstType::push_this, int(in_global));
        break;
      }
      case ASTNode::AST_EXPR_PAREN:
        visit(static_cast<ParenthesisExpr *>(node)->expr);
        break;
      case ASTNode::AST_EXPR_BOOL:
        emit(InstType::push_bool, u32(node->get_source() == u"true"));
        break;
      case ASTNode::AST_STMT_VAR:
        visit_variable_statement(*static_cast<VarStatement *>(node));
        break;
      case ASTNode::AST_STMT_VAR_DECL:
        assert(false);
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
      case ASTNode::AST_EXPR_NEW:
        visit_new_expr(*static_cast<NewExpr *>(node));
        break;
//      case ASTNode::AST_STMT_FOR:
//        break;
//      case ASTNode::AST_STMT_FOR_IN:
//        break;
//      case ASTNode::AST_STMT_SWITCH:
//        break;
      case ASTNode::AST_EXPR_TRIPLE:
        visit_ternary_expr(*static_cast<TernaryExpr *>(node));
        break;
//      case ASTNode::AST_EXPR_REGEXP:
//        break;
      case ASTNode::AST_EXPR_COMMA:
        visit_comma_expr(*static_cast<Expression *>(node));
        break;
      case ASTNode::AST_STMT_DO_WHILE:
        visit_do_while_statement(*static_cast<DoWhileStatement *>(node));
        break;
//      case ASTNode::AST_STMT_LABEL:
//        break;
      case ASTNode::AST_STMT_EMPTY:
        break;
      default:
        std::cout << node->description() << " not supported yet" << '\n';
        assert(false);
    }
  }

  void visit_program_or_function_body(ProgramOrFunctionBody& program) {

    // First, allocate space for the variables (defined by `let` and `const`) in this scope
    int deinit_begin = int(scope().get_var_next_index() + frame_meta_size);

    for (ASTNode *node : program.statements) {
      if (node->type != ASTNode::AST_STMT_VAR) continue;

      auto& var_stmt = *static_cast<VarStatement *>(node);
      for (VarDecl *decl : var_stmt.declarations) {
        if (var_stmt.kind == VarKind::DECL_LET || var_stmt.kind == VarKind::DECL_CONST) {
          bool res = scope().define_symbol(var_stmt.kind, decl->id.text);
          if (!res) {
            report_error(CodegenError {
                .message = "Duplicate variable: " + to_u8string(decl->id.text),
                .ast_node = decl,
            });
          }
        }
      }
    }
    int deinit_end = int(scope().get_var_next_index() + frame_meta_size);
    // set local `let` and `const` variables to UNINIT.
    if (deinit_end - deinit_begin != 0) {
      emit(InstType::var_deinit_range, deinit_begin, deinit_end);
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
        auto& func = *static_cast<Function *>(node);
        visit_function(func);
        auto symbol = scope().resolve_symbol(func.name.text);
        assert(!symbol.not_found());
        emit(InstType::pop, scope_type_int(symbol.storage_scope), symbol.get_index());
      }
      // End filling function symbol initialization code
    }

    std::vector<u32> top_level_throw;

    for (ASTNode *stmt : program.statements) {
      if (not stmt->is(ASTNode::AST_EXPR_ASSIGN)) {
        visit(stmt);
        // pop drop unused result #2
        if (stmt->type > ASTNode::BEGIN_EXPR && stmt->type < ASTNode::END_EXPR) {
          emit(InstType::pop_drop);
        }
      } else {
        visit_assignment_expr(*static_cast<AssignmentExpr *>(stmt), false);
      }

      if (not scope().throw_list.empty()) {
        auto& throw_list = scope().throw_list;
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

    scope().get_outer_func()->catch_table.emplace_back(0, 0, bytecode_pos());

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
        .catch_table = std::move(scope().catch_table)
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

  void visit_comma_expr(Expression& expr) {
    for (ASTNode *ele : expr.elements) {
      assert(ele->type > ASTNode::BEGIN_EXPR && ele->type < ASTNode::END_EXPR);
      visit(ele);
      emit(InstType::pop_drop);
    }

    bytecode.pop_back(); // don't pop the result of the last element. It's the result of the comma expression.
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
          if (expr.operand->is(ASTNode::AST_EXPR_BOOL)) {
            emit(InstType::push_bool, u32(not (expr.operand->get_source() == u"true")));
          } else {
            visit(expr.operand);
            emit(InstType::logi_not);
          }
        }
        else {
          assert(false);
        }
        break;
      case Token::BIT_NOT:
        if (expr.is_prefix_op) {
          visit(expr.operand);
          emit(InstType::bits_not);
        }
        else {
          assert(false);
        }
        break;
      case Token::SUB:
        if (expr.is_prefix_op) {
          if (expr.operand->is(ASTNode::AST_EXPR_NUMBER)) {
            emit(Instruction::num_imm(-expr.operand->as_number_literal()->num_val));
          } else {
            visit(expr.operand);
            emit(InstType::neg);
          }
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
        bytecode[idx].operand.two.opr2 = int(bytecode_pos());
      }
    }
    else {
      visit(expr.lhs);
      visit(expr.rhs);
      switch (expr.op.type) {
        case Token::ADD: emit(InstType::add); break;
        case Token::SUB: emit(InstType::sub); break;
        case Token::MUL: emit(InstType::mul); break;
        case Token::DIV: emit(InstType::div); break;

        case Token::BIT_AND: emit(InstType::bits_and); break;
        case Token::BIT_OR: emit(InstType::bits_or); break;
        case Token::BIT_XOR: emit(InstType::bits_xor); break;

        case Token::LSH: emit(InstType::lsh); break;
        case Token::RSH: emit(InstType::rsh); break;
        case Token::UNSIGNED_RSH: emit(InstType::ursh); break;

        case Token::NE: emit(InstType::ne); break;
        case Token::EQ: emit(InstType::eq); break;
        case Token::EQ3: emit(InstType::eq3); break;
        case Token::NE3: emit(InstType::ne3); break;

        case Token::LE: emit(InstType::le); break;
        case Token::LT: emit(InstType::lt); break;
        case Token::GT: emit(InstType::gt); break;
        case Token::GE: emit(InstType::ge); break;
        default: assert(false);
      }
    }

  }

  void visit_expr_in_logical_expr(ASTNode& expr, vector<u32>& true_list, vector<u32>& false_list,
                                  bool need_value) {
    if (expr.is_binary_logical_expr()) {
      visit_logical_expr(*expr.as_binary_expr(), true_list, false_list, need_value);
    }
    else if (expr.is_not_expr() && !need_value) {
      auto& not_expr = static_cast<UnaryExpr&>(expr);
      visit_expr_in_logical_expr(*not_expr.operand, true_list, false_list, need_value);

      vector<u32> temp = std::move(false_list);
      false_list = std::move(true_list);
      true_list = std::move(temp);
    }
    else {
      visit(&expr);
      u32 jmp_pos = emit(InstType::jmp_cond);
      true_list.push_back(jmp_pos);
      false_list.push_back(jmp_pos);
    }
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
        bytecode[idx].operand.two.opr2 = int(bytecode_pos());
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

  void visit_assignment_expr(AssignmentExpr& expr, bool need_value = true) {

    auto assign_type_to_op_type = [] (TokenType assign_type) {
      int assign_type_to_op_type = static_cast<int>(InstType::add_assign) - TokenType::ADD_ASSIGN;
      return static_cast<InstType>(assign_type + assign_type_to_op_type);
    };

    // a = b
    if (expr.is_simple_assign()) {
      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());
      auto rhs_sym = scope().resolve_symbol(expr.rhs->get_source());

      if (!lhs_sym.not_found() && !rhs_sym.not_found()) {
        emit(InstType::fast_assign, scope_type_int(lhs_sym.storage_scope), lhs_sym.get_index(),
             scope_type_int(rhs_sym.storage_scope), rhs_sym.get_index());
        if (need_value) {
          if (rhs_sym.is_let_or_const()) {
            emit(InstType::push_check, scope_type_int(rhs_sym.storage_scope), rhs_sym.get_index());
          } else {
            emit(InstType::push, scope_type_int(rhs_sym.storage_scope), rhs_sym.get_index());
          }
        }
      }
      else {
        if (lhs_sym.not_found()) {
          u32 name_atom = str_pool.atomize_sv(expr.lhs->get_source());
          emit(InstType::dyn_get_var, name_atom, (int)true);
          visit(expr.rhs);
          emit(InstType::prop_assign, (bool)need_value);
        } else {
          visit(expr.rhs);
          emit(need_value ? InstType::store : InstType::pop,
               scope_type_int(lhs_sym.storage_scope), lhs_sym.get_index());
        }
      }
    }
    // a = ... or a += ...
    else if (expr.lhs_is_id()) {
      auto lhs_sym = scope().resolve_symbol(expr.lhs->get_source());

      if (lhs_sym.not_found()) {
        u32 name_atom = str_pool.atomize_sv(expr.lhs->get_source());
        emit(InstType::dyn_get_var, name_atom, (int)true);
        visit(expr.rhs);
        if (expr.assign_type == TokenType::ASSIGN) {
          emit(InstType::prop_assign, (bool)need_value);
        } else {
          InstType assign_type = assign_type_to_op_type(expr.assign_type);
          emit(InstType::prop_compound_assign, static_cast<int>(assign_type), (int)need_value);
        }

        return;
      }

      int lhs_scope = scope_type_int(lhs_sym.storage_scope);
      int lhs_sym_index = (int)lhs_sym.get_index();
      // a = ...
      if (expr.assign_type == TokenType::ASSIGN) {
        visit(expr.rhs);
        emit(need_value ? InstType::store : InstType::pop, lhs_scope, lhs_sym_index);
      }
      // a += expr or a -= expr
      else if (expr.assign_type == TokenType::ADD_ASSIGN || expr.assign_type == TokenType::SUB_ASSIGN) {
        if (expr.rhs->is(ASTNode::AST_EXPR_NUMBER) && expr.rhs->as_number_literal()->num_val == 1) {
          InstType inst = expr.assign_type == TokenType::ADD_ASSIGN ? InstType::inc : InstType::dec;
          emit(inst, lhs_scope, lhs_sym_index);
        } else {
          visit(expr.rhs);
          emit(assign_type_to_op_type(expr.assign_type), lhs_scope, lhs_sym_index);
        }
        if (need_value) {
          emit(InstType::push, lhs_scope, lhs_sym_index);
        }
      }
      else {
        visit(expr.rhs);
        emit(assign_type_to_op_type(expr.assign_type), lhs_scope, lhs_sym_index);
        if (need_value) {
          emit(InstType::push, lhs_scope, lhs_sym_index);
        }
      }
    }
    else {
      // check if left hand side is LeftHandSide Expression (or Parenthesized Expression with
      // LeftHandSide Expression in it.
      if (expr.lhs->type == ASTNode::AST_EXPR_LHS) {
        visit_left_hand_side_expr(*static_cast<LeftHandSideExpr *>(expr.lhs), true, false);
      } else {
        assert(false);
      }

      visit(expr.rhs);
      if (expr.assign_type == Token::ASSIGN) {
        emit(InstType::prop_assign, (int)need_value);
      } else {
        InstType assign_type = assign_type_to_op_type(expr.assign_type);
        emit(InstType::prop_compound_assign, static_cast<int>(assign_type), (int)need_value);
      }
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
    emit(InstType::make_array, (int)array_lit.len);

    for (auto& [idx, element] : array_lit.elements) {
      if (element != nullptr) {
        visit(element);
      } else {
        emit(InstType::push_uninit);
      }
    }

    if (!array_lit.elements.empty()) {
      emit(InstType::add_elements, (int)array_lit.len);
    }
  }

  void visit_left_hand_side_expr(LeftHandSideExpr& expr, bool create_ref, bool in_new_ctx) {
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
        if (!in_new_ctx) emit(InstType::call, expr.args_list[idx]->args.size(), int(has_this_object));
        else emit(InstType::js_new, expr.args_list[idx]->args.size());
      }
      // obj.prop
      else if (postfix_type == LeftHandSideExpr::PROP) {
        size_t prop_start = i;
        for (; i < postfix_size && postfix_ord[i].first == LeftHandSideExpr::PROP; i++) {
          idx = postfix_ord[i].second;
          int key_id = (int)add_const(expr.prop_list[idx].text);
          emit(InstType::key_access, key_id, 0);
        }
        bytecode.back().operand.two.opr2 = int(create_ref);
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
    if (not symbol.not_found()) {
      if (symbol.is_let_or_const()) {
        emit(InstType::push_check, scope_type_int(symbol.storage_scope), symbol.get_index());
      } else {
        emit(InstType::push, scope_type_int(symbol.storage_scope), symbol.get_index());
      }
    } else {
      u32 atom = str_pool.atomize_sv(id.get_source());
      emit(InstType::dyn_get_var, atom, (int)false);
    }
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
      visit_variable_declaration(var_stmt.kind, *decl);
    }
  }

  void visit_variable_declaration(VarKind var_kind, VarDecl& var_decl) {
    if (var_decl.var_init) {
      assert(var_decl.var_init->is(ASTNode::AST_EXPR_ASSIGN));
      visit_assignment_expr(*static_cast<AssignmentExpr *>(var_decl.var_init), false);
    }
    else if (var_kind == VarKind::DECL_LET) {
      auto sym = scope().resolve_symbol(var_decl.id.text);
      emit(InstType::var_undef, int(sym.get_index()));
    }
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

    if (is_stmt_valid_in_single_stmt_ctx(stmt.if_block)) {
      visit(stmt.if_block);
    }
    else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.if_block,
      });
    }

    u32 if_end_jmp = emit(InstType::jmp);

    for (u32 idx : false_list) {
      bytecode[idx].operand.two.opr2 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    if (stmt.else_block) {
      if (is_stmt_valid_in_single_stmt_ctx(stmt.else_block)) {
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

  void visit_ternary_expr(TernaryExpr& expr) {
    vector<u32> true_list;
    vector<u32> false_list;
    visit_expr_in_logical_expr(*expr.cond_expr, true_list, false_list, false);

    for (u32 idx : true_list) {
      bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    visit(expr.true_expr);
    u32 true_end_jmp = emit(InstType::jmp);

    for (u32 idx : false_list) {
      bytecode[idx].operand.two.opr2 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    visit(expr.false_expr);
    bytecode[true_end_jmp].operand.two.opr1 = bytecode_pos();
  }

  void visit_while_statement(WhileStatement& stmt) {
    u32 loop_start = bytecode_pos();
    scope().continue_pos = loop_start;
    scope().can_break = true;
    scope().can_continue = true;

    vector<u32> true_list, false_list;
    visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);

    for (u32 idx : true_list) {
      bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    if (is_stmt_valid_in_single_stmt_ctx(stmt.body_stmt)) {
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
      bytecode[idx].operand.two.opr2 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    for (u32 idx : scope().break_list) {
      bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    scope().break_list.clear();

    scope().can_break = false;
    scope().can_continue = false;
    scope().continue_pos = -1;

  }

  void visit_do_while_statement(DoWhileStatement& stmt) {
    scope().can_continue = true;
    scope().can_break = true;

    emit(InstType::jmp, bytecode_pos() + 2); // jump over the `pop_drop`
    u32 loop_start = bytecode_pos();
    emit(InstType::pop_drop);

    if (is_stmt_valid_in_single_stmt_ctx(stmt.body_stmt)) {
      visit(stmt.body_stmt);
    }
    else {
      report_error(CodegenError {
          .message = "SyntaxError: Lexical declaration cannot appear in a single-statement context",
          .ast_node = stmt.body_stmt,
      });
    }

    for (u32 idx : scope().continue_list) {
      bytecode[idx].operand.two.opr1 = bytecode_pos();
    }
    scope().continue_list.clear();

    vector<u32> true_list;
    vector<u32> false_list;
    visit_expr_in_logical_expr(*stmt.condition_expr, true_list, false_list, false);

    for (u32 idx : true_list) {
      bytecode[idx].operand.two.opr1 = loop_start;
    }
    for (u32 idx : false_list) {
      bytecode[idx].operand.two.opr2 = bytecode_pos();
    }
    emit(InstType::pop_drop);

    for (u32 idx : scope().break_list) {
      bytecode[idx].operand.two.opr1 = bytecode_pos();
    }

    scope().can_break = false;
    scope().can_continue = false;
  }

  void visit_continue_break_statement(ContinueOrBreak& stmt) {
    if (stmt.type == ASTNode::AST_STMT_CONTINUE) {
      Scope *continue_scope = scope().resolve_continue_scope();
      assert(continue_scope != nullptr);
      if (continue_scope->continue_pos != -1) {
        emit(InstType::jmp, u32(continue_scope->continue_pos));
      } else {
        continue_scope->continue_list.push_back(emit(InstType::jmp));
      }
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
      if (!res) std::cout << "!!!!define symbol " << to_u8string(name) << " failed\n";
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

    int deinit_begin = int(scope().get_var_start_index() + frame_meta_size);
    // as for now, the only usage of extra vars is for the catch variables. and they don't need
    // to be deinit.
    int deinit_end = int(scope().get_var_next_index() + frame_meta_size - extra_var.size());
    if (deinit_end - deinit_begin != 0) {
      emit(InstType::var_deinit_range, deinit_begin, deinit_end);
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

        auto& func = *static_cast<Function *>(node);
        visit_function(func);
        auto symbol = scope().resolve_symbol(func.name.text);
        assert(!symbol.not_found());
        emit(InstType::pop, scope_type_int(symbol.storage_scope), symbol.get_index());
      }
      // End filling function symbol initialization code
    }

    for (auto *stmt : block.statements) {
      if (not stmt->is(ASTNode::AST_EXPR_ASSIGN)) {
        visit(stmt);
        // pop drop unused result #1
        if (stmt->type > ASTNode::BEGIN_EXPR && stmt->type < ASTNode::END_EXPR) {
          emit(InstType::pop_drop);
        }
      } else {
        visit_assignment_expr(*static_cast<AssignmentExpr *>(stmt), false);
      }
    }

    // dispose the scope variables after leaving the block scope
    gen_scope_var_dispose_code(scope());
    pop_scope();
  }

  void gen_scope_var_dispose_code(Scope& scope) {
//    auto& sym_table = scope.get_symbol_table();
//    for (auto& [name, sym_rec] : sym_table) {
//      if (sym_rec.var_kind == VarKind::DECL_VAR || sym_rec.var_kind == VarKind::DECL_FUNC_PARAM ||
//          sym_rec.var_kind == VarKind::DECL_FUNCTION) {
//        assert(false);
//      }
//      auto scope_type = scope.get_outer_func()->get_scope_type();
//      emit(InstType::var_dispose, scope_type_int(scope_type), sym_rec.offset_idx());
//    }
    int disp_begin = int(scope.get_var_start_index() + frame_meta_size);
    int disp_end = int(scope.get_var_next_index() + frame_meta_size);
    if (disp_end - disp_begin != 0) {
      emit(InstType::var_dispose_range, disp_begin, disp_end);
    }
  }

  void visit_try_statement(TryStatement& stmt) {
    assert(stmt.try_block->type == ASTNode::AST_STMT_BLOCK);

    bool has_catch = stmt.catch_block != nullptr;
    bool has_finally = stmt.finally_block != nullptr;
    scope().has_try = true;

    // visit the try block to emit bytecode
    u32 try_start = bytecode_pos();
    visit_block_statement(*stmt.try_block->as_block(), {});
    u32 try_end = bytecode_pos() - 1;

    // We are going to record the address of the variables in the try block.
    // When an exception happens, we should dispose the variables in this block.
    u32 var_dispose_start = scope().get_var_next_index() + frame_meta_size;
    u32 var_dispose_end = stmt.try_block->as_block()->scope->get_var_count() + frame_meta_size;

    scope().has_try = false;

    u32 try_end_jmp = emit(InstType::jmp);
    u32 catch_or_finally_pos = try_end_jmp + 1;

    // `throw` statements will jump here
    for (u32 idx : scope().throw_list) {
      bytecode[idx].operand.two.opr1 = int(catch_or_finally_pos);
    }
    scope().throw_list.clear();
    // also, any error in the try block will jump here. So we should add a catch table entry.
    auto& catch_table = scope().get_outer_func()->catch_table;
    catch_table.emplace_back(try_start, try_end, catch_or_finally_pos);
    catch_table.back().local_var_begin = var_dispose_start;
    catch_table.back().local_var_end = var_dispose_end;

    // in the case of `catch (a) ...`, we are going to store the top-of-stack value to a local
    // variable. The variable is defined as `let` inside the catch block. And since it is the
    // first variable in the catch block, we know it must be at `var_next_index`.
    if (has_catch && stmt.catch_ident.is(TokenType::IDENTIFIER)) {
      int catch_id_addr = scope().get_var_next_index() + frame_meta_size;
      emit(InstType::pop, scope_type_int(scope().get_outer_func()->get_scope_type()), catch_id_addr);
    }
    else if (!has_finally) {
      emit(InstType::pop_drop);
    }

    if (has_catch) {
      vector<pair<u16string_view, VarKind>> catch_id;
      if (stmt.catch_ident.is(TokenType::IDENTIFIER)) {
        catch_id.emplace_back(stmt.catch_ident.text, VarKind::DECL_LET);
      }

      u32 catch_start = bytecode_pos();
      var_dispose_start = scope().get_var_next_index() + frame_meta_size;

      visit_block_statement(*static_cast<Block *>(stmt.catch_block), catch_id);

      var_dispose_end = stmt.catch_block->as_block()->scope->get_var_count() + frame_meta_size;
      u32 catch_end = bytecode_pos() - 1;

      if (has_finally) {
        // the `throw` in the `catch` block should go here
        u32 finally1_start = bytecode_pos();

        for (u32 idx : scope().throw_list) {
          bytecode[idx].operand.two.opr1 = int(finally1_start);
        }
        scope().throw_list.clear();

        catch_table.emplace_back(catch_start, catch_end, finally1_start);
        catch_table.back().local_var_begin = var_dispose_start;
        catch_table.back().local_var_end = var_dispose_end;
      }
    }

    if (has_finally) {
      // finally1. If there is error in the try block (without catch) or in the catch block, go here.
      visit_block_statement(*static_cast<Block *>(stmt.finally_block), {});
      scope().throw_list.push_back( emit(InstType::jmp));

      bytecode[try_end_jmp].operand.two.opr1 = bytecode_pos();
      // finally2
      visit_block_statement(*static_cast<Block *>(stmt.finally_block), {});
    } else {
      bytecode[try_end_jmp].operand.two.opr1 = bytecode_pos();
    }
  }

  void visit_throw_statement(ThrowStatement& stmt) {
    assert(stmt.expr->is_expression());
    visit(stmt.expr);
    emit(InstType::dup_stack_top);

    Scope *scope_to_clean = &scope();
    while (true) {
      bool should_clean = scope_to_clean->get_scope_type() != ScopeType::GLOBAL &&
                          scope_to_clean->get_scope_type() != ScopeType::FUNC &&
                          !scope_to_clean->has_try;
      if (!should_clean) break;

      gen_scope_var_dispose_code(*scope_to_clean);
      scope_to_clean = scope_to_clean->get_outer();
    }

    scope().resolve_throw_list()->push_back(bytecode_pos());
    emit(InstType::jmp);
  }

  void visit_new_expr(NewExpr& expr) {
    if (expr.callee->is_identifier()) {
      visit(expr.callee);
      emit(InstType::js_new, 0);
    }
    else if (expr.callee->is_lhs_expr()) {
      visit_left_hand_side_expr(*(expr.callee->as_lhs_expr()), false, true);
      if (bytecode[bytecode_pos() - 1].op_type != InstType::js_new) {
        emit(InstType::js_new, 0);
      }
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

  void report_error(CodegenError err) {
    err.describe();
    errors.push_back(std::move(err));
  }

  u32 add_const(u16string_view str_view) {
    auto idx = str_pool.atomize_sv(str_view);
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

  // single statement context is, for example, if (cond) followed by a statement without `{}`.
  // in single statement context, `let` and `const` are not allowed.
  static bool is_stmt_valid_in_single_stmt_ctx(ASTNode *stmt) {
    if (stmt->is(ASTNode::AST_STMT_VAR)
        && static_cast<VarStatement *>(stmt)->kind != VarKind::DECL_VAR) {
      return false;
    }
    return true;
  }

  static constexpr u32 frame_meta_size {2};

  std::vector<Scope *> scope_chain;
  std::vector<Instruction> bytecode;
  SmallVector<CodegenError, 10> errors;

  // for constant
  StringPool str_pool;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  // for atom
  SmallVector<u16string, 10> atom_pool;
};

} // namespace njs

#endif // NJS_CODEGEN_VISITOR_H