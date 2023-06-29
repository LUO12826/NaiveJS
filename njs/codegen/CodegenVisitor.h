#ifndef NJS_CODEGEN_VISITOR_H
#define NJS_CODEGEN_VISITOR_H

#include "njs/parser/ast.h"

namespace njs {

class CodegenVisitor {
 public:

  void visit(ASTNode *node) {
    switch (node->get_type()) {
      case ASTNode::AST_PROGRAM:
        visit_program_or_function_body(static_cast<ProgramOrFunctionBody *>(node));
        visit_program_or_function_body(static_cast<ProgramOrFunctionBody *>(node));
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

  void visit_program_or_function_body(ProgramOrFunctionBody *program) {}
  void visit_function(Function *func) {}
  void visit_binary_expr(BinaryExpr *expr) {}
  void visit_left_hand_side_expr(LeftHandSideExpr *expr) {}
  void visit_identifier(ASTNode *id) {}
  void visit_func_arguments(Arguments *args) {}
  void visit_variable_statement(VarStatement *var_stmt) {}
  void visit_variable_declaration(VarDecl *var_decl) {}

 private:
};

} // namespace njs

#endif // NJS_CODEGEN_VISITOR_H