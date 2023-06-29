#ifndef NJS_AST_H
#define NJS_AST_H

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "njs/utils/macros.h"
#include "njs/parser/enum_strings.h"
#include "token.h"

namespace njs {

using std::pair;
using std::unique_ptr;
using std::vector;

const u32 PRINT_TREE_INDENT = 2;

class ASTNode {
 public:
  enum Type {
    AST_TOKEN = 0, // used to wrap a token
    AST_EXPR_THIS,
    AST_EXPR_ID,
    AST_EXPR_STRICT_FUTURE,

    AST_EXPR_NULL,
    AST_EXPR_BOOL,
    AST_EXPR_NUMBER,
    AST_EXPR_STRING,
    AST_EXPR_REGEXP,

    AST_EXPR_ARRAY,
    AST_EXPR_OBJ,

    AST_EXPR_PAREN, // ( Expression )

    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_TRIPLE,

    AST_EXPR_ARGS,
    AST_EXPR_LHS,
    AST_EXPR_NEW,

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
  ASTNode(Type type, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : type(type), text(source), start(start), end(end), line_start(line_start) {}
  virtual ~ASTNode(){};

  Type get_type() { return type; }
  std::u16string_view get_source() { return text; }
  const std::u16string_view &get_source_ref() { return text; }
  u32 start_pos() { return start; }
  u32 end_pos() { return end; }
  u32 get_line_start() { return line_start; }

  void set_source(const std::u16string_view &source, u32 start, u32 end, u32 line_start) {
    this->text = source;
    this->start = start;
    this->end = end;
    this->line_start = line_start;
  }

  void add_child(ASTNode *node) { children.push_back(node); }

  void print_tree(int level) {
    std::string spaces(level * PRINT_TREE_INDENT, ' ');
    std::string desc = description();
    printf("%s%s\n", spaces.c_str(), desc.c_str());
    for (ASTNode *child : children) {
      if (child) child->print_tree(level + 1);
    }
  }

  virtual std::string description() {
    std::string desc(ast_type_names[(int)type]);

    if (type == AST_EXPR_ID || type == AST_TOKEN) {
      desc += "  \"";
      desc += to_utf8_string(text);
      desc += "\"";
    }

    return desc;
  }

  bool is_illegal() { return type == AST_ILLEGAL; }

 private:
  Type type;
  std::u16string_view text;
  u32 start;
  u32 end;
  u32 line_start;
  vector<ASTNode *> children;
};

class RegExpLiteral : public ASTNode {
 public:
  RegExpLiteral(std::u16string pattern, std::u16string flag, std::u16string_view source, u32 start,
                u32 end, u32 line_start)
      : ASTNode(AST_EXPR_REGEXP, source, start, end, line_start), pattern(pattern), flag(flag) {}

  std::u16string pattern;
  std::u16string flag;
};

class ArrayLiteral : public ASTNode {
 public:
  ArrayLiteral() : ASTNode(AST_EXPR_ARRAY), len(0) {}

  ~ArrayLiteral() override {
    for (auto pair : elements) { delete pair.second; }
  }

  u32 get_length() { return len; }
  vector<pair<u32, ASTNode *>> get_elements() { return elements; }

  void add_element(ASTNode *element) {
    if (element == nullptr) { printf("warning: null element added to array literal.\n"); }
    elements.emplace_back(len, element);
    len++;
  }

  vector<pair<u32, ASTNode *>> elements;
  u32 len;
};

class ObjectLiteral : public ASTNode {
 public:
  ObjectLiteral() : ASTNode(AST_EXPR_OBJ) {}

  ~ObjectLiteral() override {
    for (auto property : properties) { delete property.value; }
  }

  struct Property {
    enum Type {
      NORMAL = 0,
      GET,
      SET,
    };

    Property(Token k, ASTNode *v, Type t) : key(k), value(v), type(t) {}

    Token key;
    ASTNode *value;
    Type type;
  };

  void set_property(const Property &p) { properties.emplace_back(p); }

  vector<Property> get_properties() { return properties; }

  u32 length() { return properties.size(); }

  vector<Property> properties;
};

class ParenthesisExpr : public ASTNode {
 public:
  ParenthesisExpr(ASTNode *expr, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_EXPR_PAREN, source, start, end, line_start), expr(expr) {}

  ~ParenthesisExpr() { delete expr; }

  ASTNode *get_expr() { return expr; }

  ASTNode *expr;
};

class BinaryExpr : public ASTNode {
 public:
  BinaryExpr(ASTNode *lhs, ASTNode *rhs, Token op, std::u16string_view source, u32 start, u32 end,
             u32 line_start)
      : ASTNode(AST_EXPR_BINARY, source, start, end, line_start), lhs(lhs), rhs(rhs), op(op) {
    add_child(lhs);
    add_child(rhs);
  }

  ~BinaryExpr() override {
    delete lhs;
    delete rhs;
  }

  bool is_assign() { return op.type == TokenType::ASSIGN; }

  std::string description() override {
    return ASTNode::description() + " oprand: " + op.get_type_string();
  }

  ASTNode *lhs;
  ASTNode *rhs;
  Token op;
};

class UnaryExpr : public ASTNode {
 public:
  UnaryExpr(ASTNode *node, Token op, bool prefix)
      : ASTNode(AST_EXPR_UNARY), operand(node), op(op), is_prefix_op(prefix) {

    add_child(operand);
  }

  ~UnaryExpr() override { delete operand; }

  std::string description() override {
    std::string desc = ASTNode::description();
    desc += "  op: " + op.get_text_utf8();
    desc += is_prefix_op ? " (prefix)" : " (suffix)";
    return desc;
  }

  ASTNode *operand;
  Token op;
  bool is_prefix_op;
};

class TernaryExpr : public ASTNode {
 public:
  TernaryExpr(ASTNode *cond, ASTNode *true_expr, ASTNode *false_expr)
      : ASTNode(AST_EXPR_TRIPLE), cond_expr(cond), true_expr(true_expr), false_expr(false_expr) {}

  ~TernaryExpr() override {
    delete cond_expr;
    delete true_expr;
    delete false_expr;
  }

  ASTNode *cond_expr;
  ASTNode *true_expr;
  ASTNode *false_expr;
};

class Expression : public ASTNode {
 public:
  Expression() : ASTNode(AST_EXPR) {}
  ~Expression() override {
    for (auto element : elements) { delete element; }
  }

  void add_element(ASTNode *element) { elements.push_back(element); }

  vector<ASTNode *> elements;
};

class Arguments : public ASTNode {
 public:
  Arguments(vector<ASTNode *> args) : ASTNode(AST_EXPR_ARGS), args(args) {}

  std::string description() override {
    return ASTNode::description() +  "  " + to_utf8_string(get_source_ref());
  }

  ~Arguments() override {
    for (auto arg : args) delete arg;
  }

  vector<ASTNode *> args;
};

class NewExpr : public ASTNode {
 public:
  template <typename... Args>
  NewExpr(ASTNode *callee, Args &&...args)
      : ASTNode(AST_EXPR_NEW, std::forward<Args>(args)...), callee(callee) {
    add_child(callee);
  }

  ~NewExpr() override { delete callee; }

  std::string description() override {
    return ASTNode::description() + "  \"" + to_utf8_string(get_source_ref()) + "\"";
  }

  ASTNode *callee;
};

class LeftHandSideExpr : public ASTNode {
 public:
  enum PostfixType {
    CALL,
    INDEX,
    PROP,
  };

  LeftHandSideExpr(ASTNode *base, u32 new_count)
      : ASTNode(AST_EXPR_LHS), base(base), new_count(new_count) {
    add_child(base);
  }

  ~LeftHandSideExpr() override {
    delete base;
    for (auto args : args_list) delete args;
    for (auto index : index_list) delete index;
  }

  std::string description() override {
    return ASTNode::description() + "  base: " + base->description();
  }

  void add_arguments(Arguments *args) {
    postfix_order.emplace_back(CALL, args_list.size());
    args_list.push_back(args);
    add_child(args);
  }

  void add_index(ASTNode *index) {
    postfix_order.emplace_back(INDEX, index_list.size());
    index_list.push_back(index);
    add_child(index);
  }

  void add_prop(Token prop_name) {
    postfix_order.emplace_back(PROP, prop_names_list.size());
    prop_names_list.push_back(prop_name.text);
    add_child(new ASTNode(AST_TOKEN, prop_name.text, prop_name.start, prop_name.end, prop_name.line));
  }

  ASTNode *base;
  u32 new_count;

  vector<pair<PostfixType, u32>> postfix_order;
  vector<Arguments *> args_list;
  vector<ASTNode *> index_list;
  vector<std::u16string_view> prop_names_list;
};

class Function : public ASTNode {
 public:
  // Function(vector<std::u16string> params, AST* body,
  //          std::u16string_view source, u32 start, u32 end) :
  //   Function(Token::none, params, body, source, start, end, line_start) {}

  Function(Token name, vector<std::u16string> params, ASTNode *body, std::u16string_view source,
           u32 start, u32 end, u32 line_start)
      : ASTNode(AST_FUNC, source, start, end, line_start), name(name), params(params) {
    ASSERT(body->get_type() == ASTNode::AST_FUNC_BODY);
    this->body = body;
    add_child(body);
  }

  ~Function() override { delete body; }

  std::string description() override {
    std::string desc = ASTNode::description() + " name: " + name.get_text_utf8();
    desc += ", params: ";
    for (auto& param: params) desc += to_utf8_string(param) + ", ";
    return desc;
  }

  bool has_name() { return name.type != TokenType::NONE; }
  std::u16string_view get_name() { return name.text; }

  Token name;
  vector<std::u16string> params;
  ASTNode *body;
};

class VarDecl : public ASTNode {
 public:
  VarDecl(Token id, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : VarDecl(id, nullptr, source, start, end, line_start) {}

  VarDecl(Token id, ASTNode *var_init, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_VAR_DECL, source, start, end, line_start), id(id), var_init(var_init) {}
  ~VarDecl() { delete var_init; }

  Token id;
  ASTNode *var_init;
};

class ProgramOrFunctionBody : public ASTNode {
 public:
  ProgramOrFunctionBody(Type type, bool strict) : ASTNode(type), strict(strict) {}

  ~ProgramOrFunctionBody() override {
    for (auto func_decl : func_decls) delete func_decl;
    for (auto stmt : stmts) delete stmt;
  }

  void add_function_decl(ASTNode *func) {
    ASSERT(func->get_type() == AST_FUNC);
    func_decls.emplace_back(static_cast<Function *>(func));
    add_child(func);
  }
  void add_statement(ASTNode *stmt) {
    stmts.emplace_back(stmt);
    add_child(stmt);
  }

  void set_var_decls(vector<VarDecl *> &&var_decls) { this->var_decls = var_decls; }

  bool strict;
  vector<ASTNode *> stmts;
  vector<Function *> func_decls;
  vector<VarDecl *> var_decls;
};

class LabelledStatement : public ASTNode {
 public:
  LabelledStatement(Token label, ASTNode *stmt, std::u16string_view source, u32 start, u32 end,
                    u32 line_start)
      : ASTNode(AST_STMT_LABEL, source, start, end, line_start), label(label), statement(stmt) {}

  ~LabelledStatement() { delete statement; }

  Token label;
  ASTNode *statement;
};

class ContinueOrBreak : public ASTNode {
 public:
  ContinueOrBreak(Type type, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ContinueOrBreak(type, Token::none, source, start, end, line_start) {}

  ContinueOrBreak(Type type, Token id, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(type, source, start, end, line_start), id(id) {}

  Token id;
};

class ReturnStatement : public ASTNode {
 public:
  ReturnStatement(ASTNode *expr, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_RETURN, source, start, end, line_start), expr(expr) {
    add_child(expr);
  }

  ~ReturnStatement() {
    delete expr;
  }

  ASTNode *expr;
};

class ThrowStatement : public ASTNode {
 public:
  ThrowStatement(ASTNode *expr, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_THROW, source, start, end, line_start), expr(expr) {}

  ~ThrowStatement() {
    delete expr;
  }

  ASTNode *expr;
};

class VarStatement : public ASTNode {
 public:
  enum VarKind {
    DECL_VAR,
    DECL_LET,
    DECL_CONST
  };

  std::string get_var_kind_str() {
    switch (kind) {
      case DECL_VAR: return "var";
      case DECL_LET: return "let";
      case DECL_CONST: return "const";
      default: return "var";
    }
  }

  VarStatement(VarKind var_kind) : ASTNode(AST_STMT_VAR), kind(var_kind) {}
  ~VarStatement() {
    for (auto decl : declarations) delete decl;
  }

  std::string description() override {
    return ASTNode::description() + "  " + get_var_kind_str();
  }

  void add_decl(ASTNode *decl) {
    ASSERT(decl->get_type() == AST_STMT_VAR_DECL);
    declarations.emplace_back(static_cast<VarDecl *>(decl));
    add_child(decl);
  }

  VarKind kind;
  vector<VarDecl *> declarations;
};

class Block : public ASTNode {
 public:
  Block() : ASTNode(AST_STMT_BLOCK) {}

  ~Block() {
    for (auto stmt : statements) { delete stmt; }
  }

  void add_statement(ASTNode *stmt) {
    statements.emplace_back(stmt);
    add_child(stmt);
  }

  vector<ASTNode *> statements;
};

class TryStatement : public ASTNode {
 public:
  TryStatement(ASTNode *try_block, Token catch_ident, ASTNode *catch_block, std::u16string_view source,
               u32 start, u32 end, u32 line_start)
      : TryStatement(try_block, catch_ident, catch_block, nullptr, source, start, end, line_start) {}

  TryStatement(ASTNode *try_block, ASTNode *finally_block, std::u16string_view source, u32 start,
               u32 end, u32 line_start)
      : TryStatement(try_block, Token::none, nullptr, finally_block, source, start,
                     end, line_start) {}

  TryStatement(ASTNode *try_block, Token catch_ident, ASTNode *catch_block, ASTNode *finally_block,
               std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_TRY, source, start, end, line_start), try_block(try_block),
        catch_ident(catch_ident), catch_block(catch_block), finally_block(finally_block) {

    add_child(try_block);
    add_child(catch_block);
    add_child(finally_block);
  }

  ~TryStatement() {
    delete try_block;
    delete catch_block;
    delete finally_block;
  }

  std::u16string_view get_catch_identifier() { return catch_ident.text; };

  ASTNode *try_block;
  Token catch_ident;
  ASTNode *catch_block;
  ASTNode *finally_block;
};

class IfStatement : public ASTNode {
 public:
  IfStatement(ASTNode *cond, ASTNode *if_block, std::u16string_view source, u32 start, u32 end,
              u32 line_start)
      : IfStatement(cond, if_block, nullptr, source, start, end, line_start) {}

  IfStatement(ASTNode *cond, ASTNode *if_block, ASTNode *else_block, std::u16string_view source,
              u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_IF, source, start, end, line_start), condition_expr(cond), if_block(if_block),
        else_block(else_block) {

    add_child(cond);
    add_child(if_block);
    add_child(else_block);
  }

  ~IfStatement() {
    delete condition_expr;
    delete if_block;
    delete else_block;
  }

  ASTNode *condition_expr;
  ASTNode *if_block;
  ASTNode *else_block;
};

class WhileStatement : public ASTNode {
 public:
  WhileStatement(ASTNode *condition_expr, ASTNode *body_stmt, std::u16string_view source, u32 start,
                 u32 end, u32 line_start)
      : ASTNode(AST_STMT_WHILE, source, start, end, line_start), condition_expr(condition_expr),
        body_stmt(body_stmt) {}

  ~WhileStatement() {
    delete condition_expr;
    delete body_stmt;
  }

  ASTNode *condition_expr;
  ASTNode *body_stmt;
};

class WithStatement : public ASTNode {
 public:
  WithStatement(ASTNode *expr, ASTNode *stmt, std::u16string_view source, u32 start, u32 end,
                u32 line_start)
      : ASTNode(AST_STMT_WITH, source, start, end, line_start), expr(expr), stmt(stmt) {}

  ~WithStatement() {
    delete expr;
    delete stmt;
  }

  ASTNode *expr;
  ASTNode *stmt;
};

class DoWhileStatement : public ASTNode {
 public:
  DoWhileStatement(ASTNode *condition_expr, ASTNode *body_stmt, std::u16string_view source, u32 start,
                   u32 end, u32 line_start)
      : ASTNode(AST_STMT_DO_WHILE, source, start, end, line_start), condition_expr(condition_expr),
        body_stmt(body_stmt) {}

  ~DoWhileStatement() {
    delete condition_expr;
    delete body_stmt;
  }

  ASTNode *condition_expr;
  ASTNode *body_stmt;
};

class SwitchStatement : public ASTNode {
 public:
  struct DefaultClause {
    vector<ASTNode *> stmts;
  };

  struct CaseClause {
    CaseClause(ASTNode *expr, vector<ASTNode *> stmts) : expr(expr), stmts(stmts) {}
    ASTNode *expr;
    vector<ASTNode *> stmts;
  };

  SwitchStatement() : ASTNode(AST_STMT_SWITCH) {}

  ~SwitchStatement() override {
    delete condition_expr;
    for (CaseClause clause : before_default_case_clauses) {
      delete clause.expr;
      for (auto stmt : clause.stmts) { delete stmt; }
    }
    for (CaseClause clause : after_default_case_clauses) {
      delete clause.expr;
      for (auto stmt : clause.stmts) { delete stmt; }
    }
    for (auto stmt : default_clause.stmts) { delete stmt; }
  }

  void set_expr(ASTNode *expr) { condition_expr = expr; }

  void set_default_clause(vector<ASTNode *> stmts) {
    ASSERT(!has_default_clause);
    has_default_clause = true;
    default_clause.stmts = stmts;
  }

  void add_before_default_clause(CaseClause c) { before_default_case_clauses.emplace_back(c); }

  void add_after_default_clause(CaseClause c) { after_default_case_clauses.emplace_back(c); }

  ASTNode *condition_expr;
  bool has_default_clause = false;
  DefaultClause default_clause;
  vector<CaseClause> before_default_case_clauses;
  vector<CaseClause> after_default_case_clauses;
};

class ForStatement : public ASTNode {
 public:
  ForStatement(vector<ASTNode *> init_expr, ASTNode *condition_expr, ASTNode *increment_expr,
               ASTNode *body_stmt, std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_FOR, source, start, end, line_start), init_expr(init_expr),
        condition_expr(condition_expr), increment_expr(increment_expr), body_stmt(body_stmt) {
    for (auto expr : init_expr) add_child(expr);
    add_child(condition_expr);
    add_child(increment_expr);
    add_child(body_stmt);
  }

  ~ForStatement() {
    for (auto expr : init_expr) delete expr;
    delete condition_expr;
    delete increment_expr;
    delete body_stmt;
  }

  vector<ASTNode *> init_expr;
  ASTNode *condition_expr;
  ASTNode *increment_expr;
  ASTNode *body_stmt;
};

class ForInStatement : public ASTNode {
 public:
  ForInStatement(ASTNode *element_expr, ASTNode *collection_expr, ASTNode *body_stmt,
                 std::u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_FOR_IN, source, start, end, line_start), element_expr(element_expr),
        collection_expr(collection_expr), body_stmt(body_stmt) {
    add_child(element_expr);
    add_child(collection_expr);
    add_child(body_stmt);
  }

  ~ForInStatement() {
    delete element_expr;
    delete collection_expr;
    delete body_stmt;
  }

  ASTNode *element_expr;
  ASTNode *collection_expr;
  ASTNode *body_stmt;
};

} // namespace njs

#endif // NJS_AST_H