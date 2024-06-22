#include "ast.h"

namespace njs {

ASTNode::ASTNode(Type type) : type(type) {}

ASTNode::ASTNode(Type type, u16string_view source, SourceLocRef start, SourceLocRef end)
    : type(type), text(source), src_start(start), src_end(end) {}

ASTNode::~ASTNode() = default;

SourceLoc ASTNode::source_start() { return src_start; }
SourceLoc ASTNode::source_end() { return src_end; }

u16string_view ASTNode::get_source() { return text; }

void ASTNode::set_source(u16string_view source, SourceLocRef start, SourceLocRef end) {
  this->text = source;
  this->src_start = start;
  this->src_end = end;
}

void ASTNode::add_child(ASTNode *node) { children.push_back(node); }
vector<ASTNode *> ASTNode::get_children() { return children; }

void ASTNode::print_tree(int level) {
  std::string spaces(level * PRINT_TREE_INDENT, ' ');
  std::string desc = description();
  printf("%s%s\n", spaces.c_str(), desc.c_str());
  for (ASTNode *child : children) {
    if (child) child->print_tree(level + 1);
  }
}

std::string ASTNode::description() {
  std::string desc(ast_type_names[(int)type]);

  if (type == EXPR_ID || type == TOKEN) {
    desc += "  \"";
    desc += to_u8string(text);
    desc += "\"";
  }

  return desc;
}


ParenthesisExpr *ASTNode::as_paren_expr() {
  return static_cast<ParenthesisExpr *>(this);
}

LeftHandSideExpr *ASTNode::as_lhs_expr() {
  return static_cast<LeftHandSideExpr *>(this);
}

BinaryExpr *ASTNode::as_binary_expr() {
  return static_cast<BinaryExpr *>(this);
}

UnaryExpr *ASTNode::as_unary_expr() {
  return static_cast<UnaryExpr *>(this);
}

ProgramOrFunctionBody *ASTNode::as_func_body() {
  return static_cast<ProgramOrFunctionBody *>(this);
}

NumberLiteral *ASTNode::as_number_literal() {
  return static_cast<NumberLiteral *>(this);
}

Block *ASTNode::as_block() {
  return static_cast<Block *>(this);
}

Function *ASTNode::as_function() {
  return static_cast<Function *>(this);
}

bool ASTNode::is_illegal() { return type == ILLEGAL; }

bool ASTNode::is_expression() {
  return type > BEGIN_EXPR && type < END_EXPR;
}

bool ASTNode::is_binary_expr() {
  return type == EXPR_BINARY;
}

bool ASTNode::is_binary_logical_expr() {
  if (!is_binary_expr()) return false;
  auto *bin_expr = static_cast<BinaryExpr *>(this);
  return bin_expr->op.is_binary_logical();
}

bool ASTNode::is_unary_expr() {
  return type == EXPR_UNARY;
}

bool ASTNode::is_not_expr() {
  if (!is_unary_expr()) return false;
  auto *unary_expr = static_cast<UnaryExpr *>(this);
  return unary_expr->op.type == Token::LOGICAL_NOT && unary_expr->is_prefix_op;
}

bool ASTNode::is_identifier() {
  return type == EXPR_ID;
}

bool ASTNode::is_lhs_expr() {
  return type == EXPR_LHS;
}

// single statement context is, for example, if (cond) followed by a statement without `{}`.
// in single statement context, `let` and `const` are not allowed.
bool ASTNode::is_valid_in_single_stmt_ctx() {
  if (is(ASTNode::STMT_VAR) && as<VarStatement>()->kind != VarKind::VAR) {
    return false;
  }
  return true;
}

bool ASTNode::is_loop() {
  return type >= STMT_WHILE && type <= STMT_FOR_IN;
}

bool ASTNode::is_block() {
  return type == STMT_BLOCK;
}

} // end namespace njs