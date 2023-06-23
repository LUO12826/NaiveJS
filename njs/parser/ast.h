#ifndef NJS_AST_H
#define NJS_AST_H

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdio>
#include <memory>

#include "token.h"
#include "njs/utils/macros.h"
#include "njs/utils/helper.h"

namespace njs {

template<typename T>
using uni_ptr = std::unique_ptr<T>;
using std::vector;
using std::pair;
using std::u16string_view;

const u32 PRINT_TREE_INDENT = 2;

const char *ast_type_names[] = {
  "AST_EXPR_THIS",
  "AST_EXPR_IDENT",
  "AST_EXPR_STRICT_FUTURE",

  "AST_EXPR_NULL",
  "AST_EXPR_BOOL",
  "AST_EXPR_NUMBER",
  "AST_EXPR_STRING",
  "AST_EXPR_REGEXP",

  "AST_EXPR_ARRAY",
  "AST_EXPR_OBJ",

  "AST_EXPR_PAREN",  // ( Expression )

  "AST_EXPR_BINARY",
  "AST_EXPR_UNARY",
  "AST_EXPR_TRIPLE",

  "AST_EXPR_ARGS",
  "AST_EXPR_LHS",

  "AST_EXPR",

  "AST_FUNC",

  "AST_STMT_EMPTY",
  "AST_STMT_BLOCK",
  "AST_STMT_IF",
  "AST_STMT_WHILE",
  "AST_STMT_FOR",
  "AST_STMT_FOR_IN",
  "AST_STMT_WITH",
  "AST_STMT_DO_WHILE",
  "AST_STMT_TRY",

  "AST_STMT_VAR",
  "AST_STMT_VAR_DECL",

  "AST_STMT_CONTINUE",
  "AST_STMT_BREAK",
  "AST_STMT_RETURN",
  "AST_STMT_THROW",

  "AST_STMT_SWITCH",

  "AST_STMT_LABEL",
  "AST_STMT_DEBUG",

  "AST_PROGRAM",
  "AST_FUNC_BODY",

  "AST_ILLEGAL"
};

class ASTNode {
 public:
  enum Type {
    AST_EXPR_THIS = 0,
    AST_EXPR_IDENT,
    AST_EXPR_STRICT_FUTURE,

    AST_EXPR_NULL,
    AST_EXPR_BOOL,
    AST_EXPR_NUMBER,
    AST_EXPR_STRING,
    AST_EXPR_REGEXP,

    AST_EXPR_ARRAY,
    AST_EXPR_OBJ,

    AST_EXPR_PAREN,  // ( Expression )

    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_TRIPLE,

    AST_EXPR_ARGS,
    AST_EXPR_LHS,

    AST_EXPR,

    AST_FUNC,

    AST_STMT_EMPTY,
    AST_STMT_BLOCK,
    AST_STMT_IF,
    AST_STMT_WHILE,
    AST_STMT_FOR,
    AST_STMT_FOR_IN,
    AST_STMT_WITH,
    AST_STMT_DO_WHILE,
    AST_STMT_TRY,

    AST_STMT_VAR,
    AST_STMT_VAR_DECL,

    AST_STMT_CONTINUE,
    AST_STMT_BREAK,
    AST_STMT_RETURN,
    AST_STMT_THROW,

    AST_STMT_SWITCH,

    AST_STMT_LABEL,
    AST_STMT_DEBUG,

    AST_PROGRAM,
    AST_FUNC_BODY,

    AST_ILLEGAL,
  };

  ASTNode(Type type) : type(type) {}
  ASTNode(Type type, u16string_view source, u32 start, u32 end, u32 line_start) :
    type(type), text(source), start(start), end(end), line_start(line_start) {}
  virtual ~ASTNode() {};

  Type get_type() { return type; }
  u16string_view source() { return text; }
  const u16string_view& source_ref() { return text; }
  u32 start_pos() { return start; }
  u32 end_pos() { return end; }
  u32 get_line_start() { return line_start; }

  void set_source(const u16string_view& source, u32 start, u32 end, u32 line_start) {
    this->text = source;
    this->start = start;
    this->end = end;
    this->line_start = line_start;
  }

  void add_child(ASTNode *node) {

    children.push_back(node);
  }

  void print_tree(int level) {
    std::string spaces(level * PRINT_TREE_INDENT, ' ');
    std::string desc = description();
    printf("%s%s\n", spaces.c_str(), desc.c_str());
    for (ASTNode *child : children) {
      if (child) child->print_tree(level + 1);
    }
  }

  std::string description() {
    return std::string(ast_type_names[(int)type]);
  }

  bool is_illegal() { return type == AST_ILLEGAL; }

  Type type;
  u16string_view text;
  u32 start;
  u32 end;
  u32 line_start;
  vector<ASTNode*> children;
};

class RegExpLiteral : public ASTNode {
 public:
  RegExpLiteral(std::u16string pattern, std::u16string flag,
                u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_EXPR_REGEXP, source, start, end, line_start), pattern(pattern), flag(flag) {}

  std::u16string pattern;
  std::u16string flag;
};

class ArrayLiteral : public ASTNode {
 public:
  ArrayLiteral() : ASTNode(AST_EXPR_ARRAY) {}

  u32 get_length() { return elements.size(); }
  const vector<uni_ptr<ASTNode>>& get_elements() { return elements; }

  void add_element(uni_ptr<ASTNode> element) {
    if (element == nullptr) {
      printf("warning: null element added to array literal.\n");
    }
    elements.push_back(std::move(element));
  }

  vector<uni_ptr<ASTNode>> elements;
};

class ObjectLiteral : public ASTNode {
 public:
  ObjectLiteral() : ASTNode(AST_EXPR_OBJ) {}

  struct Property {
    enum Type {
      NORMAL = 0,
      GETTER,
      SETTER,
    };

    Property(Token key, uni_ptr<ASTNode> value, Type t)
              : key(key), value(std::move(value)), type(t) {}

    Type type;
    Token key;
    uni_ptr<ASTNode> value;
  };

  void set_property(Property p) {
    properties.push_back(std::move(p));
  }

  u32 length() { return properties.size(); }

  vector<Property> properties;
};

class ParenthesisExpr : public ASTNode {
 public:
  ParenthesisExpr(uni_ptr<ASTNode> expr, u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_EXPR_PAREN, source, start, end, line_start), expr(std::move(expr)) {}

  uni_ptr<ASTNode> expr;
};

class BinaryExpr : public ASTNode {
 public:
  BinaryExpr(uni_ptr<ASTNode> lhs, uni_ptr<ASTNode> rhs, Token op,
              u16string_view source, u32 start, u32 end, u32 line_start)
              : ASTNode(AST_EXPR_BINARY, source, start, end, line_start),
              lhs(std::move(lhs)), rhs(std::move(lhs)), op(op) {}

  uni_ptr<ASTNode> lhs;
  uni_ptr<ASTNode> rhs;
  Token op;
};

class UnaryExpr : public ASTNode {
 public:
  UnaryExpr(uni_ptr<ASTNode> operand, Token op, bool prefix) :
    ASTNode(AST_EXPR_UNARY), operand(std::move(operand)), op(op), is_prefix_op(prefix) {}

  uni_ptr<ASTNode> operand;
  Token op;
  bool is_prefix_op;
};

class TernaryExpr : public ASTNode {
 public:
  TernaryExpr(uni_ptr<ASTNode> cond, uni_ptr<ASTNode> true_expr, uni_ptr<ASTNode> false_expr) :
    ASTNode(AST_EXPR_TRIPLE), cond_expr(std::move(cond)), true_expr(std::move(true_expr)),
    false_expr(std::move(false_expr)) {}

  uni_ptr<ASTNode> cond_expr;
  uni_ptr<ASTNode> true_expr;
  uni_ptr<ASTNode> false_expr;
};

class Expression : public ASTNode {
 public:
  Expression() : ASTNode(AST_EXPR) {}

  void add_element(uni_ptr<ASTNode> element) { elements.push_back(std::move(element)); }

  vector<uni_ptr<ASTNode>> elements;
};

class Arguments : public ASTNode {
 public:
  Arguments(vector<uni_ptr<ASTNode>>&& args) : ASTNode(AST_EXPR_ARGS), args(std::move(args)) {}

  vector<uni_ptr<ASTNode>> args;
};

class LeftHandSideExpr : public ASTNode {
 public:

   enum PostfixType{
    CALL,
    INDEX,
    PROP,
  };

  LeftHandSideExpr(uni_ptr<ASTNode> base, u32 new_count) :
    ASTNode(AST_EXPR_LHS), base(std::move(base)), new_count(new_count) {}

  void AddArguments(uni_ptr<Arguments> args) {
    postfix_order.emplace_back(CALL, args_list.size());
    args_list.push_back(std::move(args));
  }

  void AddIndex(uni_ptr<ASTNode> index) {
    postfix_order.emplace_back(INDEX, index_list.size());
    index_list.push_back(std::move(index));
  }

  void AddProp(Token prop_name) {
    postfix_order.emplace_back(PROP, prop_names_list.size());
    prop_names_list.push_back(prop_name.text);
  }

  uni_ptr<ASTNode> base;
  u32 new_count;

  vector<std::pair<PostfixType, u32>> postfix_order;
  vector<uni_ptr<Arguments>> args_list;
  vector<uni_ptr<ASTNode>> index_list;
  vector<std::u16string_view> prop_names_list;
};

class Function : public ASTNode {
 public:

  Function(Token name, vector<std::u16string> params, uni_ptr<ASTNode> body,
           u16string_view source, u32 start, u32 end, u32 line_start) :
           ASTNode(AST_FUNC, source, start, end, line_start), name(name), params(std::move(params)) {

      ASSERT(body->get_type() == ASTNode::AST_FUNC_BODY);
      this->body = std::move(body);
      add_child(body.get());
    }


  bool has_name() { return name.type != TokenType::NONE; }
  u16string_view get_name() { return name.text; }

  Token name;
  vector<std::u16string> params;
  uni_ptr<ASTNode> body;
};

class VarDecl : public ASTNode {
 public:
  VarDecl(Token id, u16string_view source, u32 start, u32 end, u32 line_start) :
    VarDecl(id, nullptr, source, start, end, line_start) {}

  VarDecl(Token id, uni_ptr<ASTNode> var_init, u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_STMT_VAR_DECL, source, start, end, line_start), id(id), var_init(std::move(var_init)) {}

  Token id;
  uni_ptr<ASTNode> var_init;
};

class ProgramOrFunctionBody : public ASTNode {
 public:
  ProgramOrFunctionBody(Type type, bool strict) : ASTNode(type), strict(strict) {}

  void add_function_decl(uni_ptr<ASTNode> func) {
    ASSERT(func->get_type() == AST_FUNC);
    func_decls.push_back(static_ptr_cast<Function>(std::move(func)));
    add_child(func.get());
  }
  void add_statement(uni_ptr<ASTNode> stmt) {
    stmts.push_back(std::move(stmt));
    add_child(stmt.get());
  }

  void set_var_decls(vector<VarDecl*> var_decls) {
    this->var_decls = std::move(var_decls);
  }

  bool strict;
  vector<uni_ptr<Function>> func_decls;
  vector<uni_ptr<ASTNode>> stmts;

  vector<VarDecl*> var_decls;
};

class LabelledStatement : public ASTNode {
 public:
  LabelledStatement(Token label, uni_ptr<ASTNode> stmt, u16string_view source,
                    u32 start, u32 end, u32 line_start) :
                    ASTNode(AST_STMT_LABEL, source, start, end, line_start),
                            label(label), statement(std::move(stmt)) {}

  Token label;
  uni_ptr<ASTNode> statement;
};

class ContinueOrBreak : public ASTNode {
 public:
  ContinueOrBreak(Type type, u16string_view source, u32 start, u32 end, u32 line_start) :
    ContinueOrBreak(type, Token(TokenType::NONE, u"", 0, 0), source, start, end, line_start) {}

  ContinueOrBreak(Type type, Token id, u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(type, source, start, end, line_start), id(id) {}

  Token id;
};

class ReturnStatement : public ASTNode {
 public:
  ReturnStatement(uni_ptr<ASTNode> expr, u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_STMT_RETURN, source, start, end, line_start), expr(std::move(expr)) {}

  uni_ptr<ASTNode> expr;
};

class ThrowStatement : public ASTNode {
 public:
  ThrowStatement(uni_ptr<ASTNode> expr, u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_STMT_THROW, source, start, end, line_start), expr(std::move(expr)) {}

  uni_ptr<ASTNode> expr;
};

class VarStatement : public ASTNode {
 public:
  VarStatement() : ASTNode(AST_STMT_VAR) {}

  void add_decl(uni_ptr<ASTNode> decl) {
    ASSERT(decl->get_type() == AST_STMT_VAR_DECL);
    declarations.push_back(static_ptr_cast<VarDecl>(std::move(decl)));
  }

  vector<uni_ptr<VarDecl>> declarations;
};

class Block : public ASTNode {
 public:
  Block() : ASTNode(AST_STMT_BLOCK) {}

  void add_statement(uni_ptr<ASTNode> stmt) {
    statements.push_back(std::move(stmt));
    add_child(stmt.get());
  }

  vector<uni_ptr<ASTNode>> statements;
};

class TryStatement : public ASTNode {
 public:

  TryStatement(uni_ptr<ASTNode> try_block, Token catch_ident, uni_ptr<ASTNode> catch_block,
              uni_ptr<ASTNode> finally_block, u16string_view source, u32 start, u32 end, u32 line_start)
              : ASTNode(AST_STMT_TRY, source, start, end, line_start),
                  try_block(std::move(try_block)), catch_ident(std::move(catch_ident)),
                  catch_block(std::move(catch_block)), finally_block(std::move(finally_block)) {
    
    add_child(try_block.get());
    add_child(catch_block.get());
    add_child(finally_block.get());
  }

  u16string_view get_catch_identifier() { return catch_ident.text; };

  uni_ptr<ASTNode> try_block;
  Token catch_ident;
  uni_ptr<ASTNode> catch_block;
  uni_ptr<ASTNode> finally_block;
};

class IfStatement : public ASTNode {
 public:
  IfStatement(uni_ptr<ASTNode> cond, uni_ptr<ASTNode> if_block, uni_ptr<ASTNode> else_block,
              u16string_view source, u32 start, u32 end, u32 line_start) :
              ASTNode(AST_STMT_IF, source, start, end, line_start), condition_expr(std::move(cond)),
              if_block(std::move(if_block)), else_block(std::move(else_block)) {
    
    add_child(cond.get());
    add_child(if_block.get());
    add_child(else_block.get());
  }

  uni_ptr<ASTNode> condition_expr;
  uni_ptr<ASTNode> if_block;
  uni_ptr<ASTNode> else_block;
};

class WhileStatement : public ASTNode {
 public:
  WhileStatement(uni_ptr<ASTNode> condition_expr, uni_ptr<ASTNode> body_stmt, u16string_view source,
                  u32 start, u32 end, u32 line_start) :
                  ASTNode(AST_STMT_WHILE, source, start, end, line_start),
                           condition_expr(std::move(condition_expr)), body_stmt(std::move(body_stmt)) {}

  uni_ptr<ASTNode> condition_expr;
  uni_ptr<ASTNode> body_stmt;
};

class WithStatement : public ASTNode {
 public:
  WithStatement(uni_ptr<ASTNode> expr, uni_ptr<ASTNode> stmt, u16string_view source,
                u32 start, u32 end, u32 line_start) :
                ASTNode(AST_STMT_WITH, source, start, end, line_start),
                expr(std::move(expr)), stmt(std::move(stmt)) {}

  uni_ptr<ASTNode> expr;
  uni_ptr<ASTNode> stmt;
};

class DoWhileStatement : public ASTNode {
 public:
  DoWhileStatement(uni_ptr<ASTNode> condition_expr, uni_ptr<ASTNode> body_stmt,
                  u16string_view source, u32 start, u32 end, u32 line_start) :
                  ASTNode(AST_STMT_DO_WHILE, source, start, end, line_start),
                  condition_expr(std::move(condition_expr)), body_stmt(std::move(body_stmt)) {}

  uni_ptr<ASTNode> condition_expr;
  uni_ptr<ASTNode> body_stmt;
};

class SwitchStatement : public ASTNode {
 public:
  struct DefaultClause {
    vector<uni_ptr<ASTNode>> stmts;
  };

  struct CaseClause {
    CaseClause(uni_ptr<ASTNode> expr, vector<uni_ptr<ASTNode>>&& stmts) :
                expr(std::move(expr)), stmts(std::move(stmts)) {}
    uni_ptr<ASTNode> expr;
    vector<uni_ptr<ASTNode>> stmts;
  };

  SwitchStatement() : ASTNode(AST_STMT_SWITCH) {}

  void SetExpr(uni_ptr<ASTNode> expr) {
    condition_expr = std::move(expr);
  }

  void SetDefaultClause(vector<uni_ptr<ASTNode>>&& stmts) {
    ASSERT(!has_default_clause);
    has_default_clause = true;
    default_clause.stmts = std::move(stmts);
  }

  void AddBeforeDefaultCaseClause(CaseClause c) {
    before_default_clauses.push_back(std::move(c));
  }

  void AddAfterDefaultCaseClause(CaseClause c) {
    after_default_clauses.push_back(std::move(c));
  }

  uni_ptr<ASTNode> condition_expr;
  bool has_default_clause = false;
  DefaultClause default_clause;
  vector<CaseClause> before_default_clauses;
  vector<CaseClause> after_default_clauses;
};

class ForStatement : public ASTNode {
 public:
  ForStatement(vector<uni_ptr<ASTNode>>&& init_expr, uni_ptr<ASTNode>&& condition_expr, uni_ptr<ASTNode>&& increment_expr,
              uni_ptr<ASTNode> body_stmt, u16string_view source, u32 start, u32 end, u32 line_start) :
              ASTNode(AST_STMT_FOR, source, start, end, line_start), init_expr(std::move(init_expr)),
              condition_expr(std::move(condition_expr)), increment_expr(std::move(increment_expr)), body_stmt(std::move(body_stmt)) {}

  vector<uni_ptr<ASTNode>> init_expr;
  uni_ptr<ASTNode> condition_expr;
  uni_ptr<ASTNode> increment_expr;

  uni_ptr<ASTNode> body_stmt;
};

class ForInStatement : public ASTNode {
 public:
  ForInStatement(uni_ptr<ASTNode> element_expr, uni_ptr<ASTNode> collection_expr, uni_ptr<ASTNode> body_stmt,
                u16string_view source, u32 start, u32 end, u32 line_start) :
                ASTNode(AST_STMT_FOR_IN, source, start, end, line_start), element_expr(std::move(element_expr)),
                collection_expr(std::move(collection_expr)), body_stmt(std::move(body_stmt)) {}

  uni_ptr<ASTNode> element_expr;
  uni_ptr<ASTNode> collection_expr;

  uni_ptr<ASTNode> body_stmt;
};

}  // namespace njs

#endif  // NJS_AST_H