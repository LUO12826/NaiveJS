#include "ast.h"

namespace njs {

ASTNode::ASTNode(Type type) : type(type) {}

ASTNode::ASTNode(Type type, std::u16string_view source, u32 start, u32 end, u32 line_start)
    : type(type), text(source), start(start), end(end), line_start(line_start) {}

ASTNode::~ASTNode() = default;

u32 ASTNode::start_pos() { return start; }
u32 ASTNode::end_pos() { return end; }
u32 ASTNode::get_line_start() { return line_start; }

const std::u16string_view &ASTNode::get_source() { return text; }
void ASTNode::set_source(const std::u16string_view &source, u32 start, u32 end, u32 line_start) {
  this->text = source;
  this->start = start;
  this->end = end;
  this->line_start = line_start;
}

void ASTNode::add_child(ASTNode *node) { children.push_back(node); }

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

  if (type == AST_EXPR_ID || type == AST_TOKEN) {
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

bool ASTNode::is_illegal() { return type == AST_ILLEGAL; }

bool ASTNode::is_expression() {
  return type > BEGIN_EXPR && type < END_EXPR;
}

bool ASTNode::is_binary_expr() {
  return type == AST_EXPR_BINARY;
}

bool ASTNode::is_binary_logical_expr() {
  if (!is_binary_expr()) return false;
  auto *bin_expr = static_cast<BinaryExpr *>(this);
  return bin_expr->op.is_binary_logical();
}

bool ASTNode::is_unary_expr() {
  return type == AST_EXPR_UNARY;
}

bool ASTNode::is_not_expr() {
  if (!is_unary_expr()) return false;
  auto *unary_expr = static_cast<UnaryExpr *>(this);
  return unary_expr->op.type == Token::LOGICAL_NOT && unary_expr->is_prefix_op;
}

bool ASTNode::is_identifier() {
  return type == AST_EXPR_ID;
}

bool ASTNode::is_lhs_expr() {
  return type == AST_EXPR_LHS;
}

} // end namespace njs