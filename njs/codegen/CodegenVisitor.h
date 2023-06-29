#ifndef NJS_CODEGEN_VISITOR_H
#define NJS_CODEGEN_VISITOR_H

#include "Scope.h"
#include "njs/include/SmallVector.h"
#include "njs/parser/ast.h"

namespace njs {

class CodegenVisitor {
 public:
  void visit(ASTNode *node) {
    switch (node->get_type()) {
      case ASTNode::AST_PROGRAM:
        visit_program_or_function_body(*static_cast<ProgramOrFunctionBody *>(node));
        visit_program_or_function_body(*static_cast<ProgramOrFunctionBody *>(node));
        break;
      case ASTNode::AST_FUNC:
        visit_function(static_cast<Function *>(node));
        break;
      case ASTNode::AST_EXPR_BINARY:
        visit_binary_expr(static_cast<BinaryExpr *>(node));
        break;
      case ASTNode::AST_EXPR_LHS:
        visit_left_hand_side_expr(static_cast<LeftHandSideExpr *>(node));
        break;
      case ASTNode::AST_EXPR_ID:
        visit_identifier(node);
        break;
      case ASTNode::AST_EXPR_ARGS:
        visit_func_arguments(static_cast<Arguments *>(node));
        break;
      case ASTNode::AST_STMT_VAR:
        visit_variable_statement(static_cast<VarStatement *>(node));
        break;
      case ASTNode::AST_STMT_VAR_DECL:
        visit_variable_declaration(static_cast<VarDecl *>(node));
        break;
      default:
        assert(false);
    }
  }

  void visit_program_or_function_body(ProgramOrFunctionBody& program) {
    if (program.type == ASTNode::AST_PROGRAM) {
      push_scope(Scope::GLOBAL_SCOPE);
    }

    // var hoisting
    for (VarDecl *var : program.var_decls) {
      current_scope().define_symbol(VarKind::DECL_VAR, var->id.text);
    }

    // function name hoisting
    for (Function *func : program.func_decls) {
      current_scope().define_symbol(VarKind::DECL_FUNCTION, func->name.text);
    }

    for (ASTNode *node : program.stmts) {
      visit(node);
    }
  }

  void visit_function(Function *func) {}

  void visit_binary_expr(BinaryExpr *expr) {}

  void visit_left_hand_side_expr(LeftHandSideExpr *expr) {}

  void visit_identifier(ASTNode *id) {}

  void visit_func_arguments(Arguments *args) {}

  void visit_variable_statement(VarStatement *var_stmt) {}

  void visit_variable_declaration(VarDecl *var_decl) {}

 private:
  SmallVector<Scope, 10> scope_chain;

  Scope& current_scope() { return scope_chain.back(); }

  void push_scope(Scope::Type scope_type) {
    scope_chain.emplace_back(scope_type);
  }
};

} // namespace njs

#endif // NJS_CODEGEN_VISITOR_H