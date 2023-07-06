#include "ast.h"

namespace njs {

ASTNode::ASTNode(Type type) : type(type) {}

ASTNode::ASTNode(Type type, std::u16string_view source, u32 start, u32 end, u32 line_start)
    : type(type), text(source), start(start), end(end), line_start(line_start) {}

ASTNode::~ASTNode() = default;

ASTNode::Type ASTNode::get_type() { return type; }
const std::u16string_view &ASTNode::get_source() { return text; }
u32 ASTNode::start_pos() { return start; }
u32 ASTNode::end_pos() { return end; }
u32 ASTNode::get_line_start() { return line_start; }

void ASTNode::set_source(const std::u16string_view &source, u32 start, u32 end, u32 line_start) {
  this->text = source;
  this->start = start;
  this->end = end;
  this->line_start = line_start;
}

void ASTNode::add_child(ASTNode *node) { children.push_back(node); }

ParenthesisExpr *ASTNode::as_paren_expr() {
  return static_cast<ParenthesisExpr *>(this);
}

LeftHandSideExpr *ASTNode::as_lhs_expr() {
  return static_cast<LeftHandSideExpr *>(this);
}

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
    desc += to_utf8_string(text);
    desc += "\"";
  }

  return desc;
}

bool ASTNode::is_illegal() { return type == AST_ILLEGAL; }

} // end namespace njs