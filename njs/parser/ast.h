#ifndef NJS_AST_H
#define NJS_AST_H

#include <cstdio>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <optional>

#include "njs/codegen/Scope.h"
#include "njs/common/enum_strings.h"
#include "njs/common/enums.h"
#include "njs/utils/macros.h"
#include "Token.h"

namespace njs {

const u32 PRINT_TREE_INDENT = 2;

using std::pair;
using std::vector;
using std::optional;
using std::unique_ptr;
using std::u16string;
using std::u16string_view;
using TokenType = Token::TokenType;

class BinaryExpr;
class UnaryExpr;
class LeftHandSideExpr;
class ParenthesisExpr;
class Function;
class ProgramOrFunctionBody;
class NumberLiteral;
class Block;

class ASTNode {
 public:
  // has corresponding string representation, note to modify when adding
  enum Type {
    AST_TOKEN = 0, // used to wrap a token

    BEGIN_EXPR,

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
    AST_EXPR_ASSIGN,
    AST_EXPR_UNARY,
    AST_EXPR_TRIPLE,

    AST_EXPR_ARGS,
    AST_EXPR_LHS,
    AST_EXPR_NEW,

    AST_EXPR_COMMA, // expr, expr, expr

    END_EXPR,

    AST_FUNC,

    AST_STMT_EMPTY,
    AST_STMT_BLOCK,
    AST_STMT_IF,
    AST_STMT_WHILE,
    AST_STMT_DO_WHILE,
    AST_STMT_FOR,
    AST_STMT_FOR_IN,
    AST_STMT_WITH,
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

  explicit ASTNode(Type type);
  ASTNode(Type type, u16string_view source, u32 start, u32 end, u32 line_start);
  virtual ~ASTNode();

  const u16string_view &get_source();
  void set_source(const u16string_view &source, u32 start, u32 end, u32 line_start);

  u32 start_pos();
  u32 end_pos();
  u32 start_line_num();

  // for printing the AST
  void add_child(ASTNode *node);
  void print_tree(int level);
  virtual std::string description();

  ParenthesisExpr *as_paren_expr();
  LeftHandSideExpr *as_lhs_expr();
  BinaryExpr *as_binary_expr();
  UnaryExpr *as_unary_expr();
  ProgramOrFunctionBody *as_func_body();
  NumberLiteral *as_number_literal();
  Block *as_block();
  Function *as_function();

  template<typename T>
  T *as() {
    return static_cast<T*>(this);
  }

  bool is(Type t) const { return this->type == t; }
  bool is_illegal();
  bool is_expression();
  bool is_binary_expr();
  bool is_binary_logical_expr();
  bool is_unary_expr();
  bool is_not_expr();
  bool is_identifier();
  bool is_lhs_expr();
  bool is_valid_in_single_stmt_ctx();
  bool is_loop();
  bool is_block();

  Type type;
  u16string_view label;
 private:
  u16string_view text;
  u32 start;
  u32 end;
  u32 line_start;
  vector<ASTNode *> children;
};

class NumberLiteral : public ASTNode {
 public:
  NumberLiteral(double val, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_EXPR_NUMBER, source, start, end, line_start), num_val(val) {}

  std::string description() override {
    return ASTNode::description() + " " + std::to_string(num_val);
  }

  double num_val;
};

class StringLiteral : public ASTNode {
 public:
  StringLiteral(u16string str, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_EXPR_STRING, source, start, end, line_start), str_val(std::move(str)) {}

  std::string description() override {
    auto desc = ASTNode::description();
    if (str_val.size() < 80) {
      desc += to_u8string(str_val);
    }
    return desc;
  }

  u16string str_val;
};

class RegExpLiteral : public ASTNode {
 public:
  RegExpLiteral(u16string pattern, u16string flag, u16string_view source, u32 start,
                u32 end, u32 line_start)
      : ASTNode(AST_EXPR_REGEXP, source, start, end, line_start),
        pattern(std::move(pattern)), flag(std::move(flag)) {}

  u16string pattern;
  u16string flag;
};

class ArrayLiteral : public ASTNode {
 public:
  ArrayLiteral() : ASTNode(AST_EXPR_ARRAY) {}

  ~ArrayLiteral() override {
    for (auto [idx, element] : elements) { delete element; }
  }

  void add_element(ASTNode *element) {
    elements.emplace_back(len, element);
    len++;
  }

  vector<pair<u32, ASTNode *>> elements;
  u32 len {0};
};

class ObjectLiteral : public ASTNode {
 public:

  struct Property {
    enum Type : int8_t {
      NORMAL = 0,
      GETTER,
      SETTER,
    };

    Property(const Token& key, ASTNode *val, Type type) : key(key), value(val), type(type) {}

    Token key;
    ASTNode *value;
    Type type;
  };

  ObjectLiteral() : ASTNode(AST_EXPR_OBJ) {}

  ~ObjectLiteral() override {
    for (auto property : properties) { delete property.value; }
  }

  void set_property(const Property &p) { properties.emplace_back(p); }

  vector<Property> properties;
};

class ParenthesisExpr : public ASTNode {
 public:
  ParenthesisExpr(ASTNode *expr, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_EXPR_PAREN, source, start, end, line_start), expr(expr) {
    add_child(expr);
  }

  ~ParenthesisExpr() override { delete expr; }

  ASTNode *expr;
};


class UnaryExpr : public ASTNode {
 public:
  UnaryExpr(ASTNode *node, const Token& op, bool prefix)
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

  bool is_inc_or_dec() const {
    return op.type == Token::INC || op.type == Token::DEC;
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
  Expression() : ASTNode(AST_EXPR_COMMA) {}
  ~Expression() override {
    for (auto element : elements) { delete element; }
  }

  void add_element(ASTNode *element) {
    elements.push_back(element);
    add_child(element);
  }

  vector<ASTNode *> elements;
};

class Arguments : public ASTNode {
 public:
  explicit Arguments(vector<ASTNode *> args)
      : ASTNode(AST_EXPR_ARGS), args(std::move(args)) {}

  ~Arguments() override {
    for (auto arg : args) delete arg;
  }

  std::string description() override {
    return ASTNode::description() + "  " + to_u8string(get_source());
  }

  u32 arg_count() {
    return args.size();
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
    return ASTNode::description() + "  \"" + to_u8string(get_source()) + "\"";
  }

  ASTNode *callee;
};

class LeftHandSideExpr : public ASTNode {
 public:
  enum PostfixType : int8_t {
    CALL,
    INDEX,
    PROP,
  };

  struct Postfix {
    PostfixType type;
    union Subtree {
      Arguments *args_expr;
      ASTNode *index_expr;
      u16string_view prop_name;

      Subtree(): prop_name() {}
    } subtree;

    Postfix(PostfixType type): type(type) {}
  };

  explicit LeftHandSideExpr(ASTNode *base) : ASTNode(AST_EXPR_LHS), base(base) {
    add_child(base);
  }

  ~LeftHandSideExpr() override {
    delete base;
    for (auto& post : postfixs) {
      if (post.type == CALL) delete post.subtree.args_expr;
      if (post.type == INDEX) delete post.subtree.index_expr;
    }
  }

  std::string description() override {
    return ASTNode::description() + "  base: " + base->description();
  }

  void add_arguments(Arguments *args) {
    Postfix post(CALL);
    post.subtree.args_expr = args;
    postfixs.push_back(post);
    add_child(args);
  }

  void add_index(ASTNode *index) {
    Postfix post(INDEX);
    post.subtree.index_expr = index;
    postfixs.push_back(post);
    add_child(index);
  }

  void add_prop(const Token& prop) {
    Postfix post(PROP);
    post.subtree.prop_name = prop.text;
    postfixs.push_back(post);
    add_child(new ASTNode(AST_TOKEN, prop.text, prop.start, prop.end, prop.line));
  }

  bool is_id() {
    return base->type == ASTNode::AST_EXPR_ID && postfixs.empty();
  }

  ASTNode *base;

  vector<Postfix> postfixs;
};

class BinaryExpr : public ASTNode {
 public:
  BinaryExpr(ASTNode *lhs, ASTNode *rhs, const Token& op, u16string_view source, u32 start, u32 end,
             u32 line_start)
      : ASTNode(AST_EXPR_BINARY, source, start, end, line_start), lhs(lhs), rhs(rhs), op(op) {
    add_child(lhs);
    add_child(rhs);
  }

  ~BinaryExpr() override {
    delete lhs;
    delete rhs;
  }

  bool is_simple_expr() {
    bool lhs_simple = lhs->type == AST_EXPR_ID || (lhs->type == ASTNode::AST_EXPR_LHS
                                                   &&
                                                   static_cast<LeftHandSideExpr *>(lhs)->is_id());
    if (!lhs_simple) return false;
    bool rhs_simple = rhs->type == AST_EXPR_ID || (rhs->type == ASTNode::AST_EXPR_LHS
                                                   &&
                                                   static_cast<LeftHandSideExpr *>(rhs)->is_id());
    return rhs_simple;
  }

  std::string description() override {
    return ASTNode::description() + " operand: " + op.get_type_string();
  }

  ASTNode *lhs;
  ASTNode *rhs;
  Token op;
};

class AssignmentExpr : public ASTNode {
 public:
  AssignmentExpr(TokenType assign_type, ASTNode *lhs, ASTNode *rhs, u16string_view source,
                 u32 start, u32 end, u32 line_start)
      : ASTNode(AST_EXPR_ASSIGN, source, start, end, line_start),
        assign_type(assign_type), lhs(lhs), rhs(rhs) {
    add_child(lhs);
    add_child(rhs);
  }

  ~AssignmentExpr() override {
    delete lhs;
    delete rhs;
  }

  bool is_simple_assign() {
    return assign_type == TokenType::ASSIGN && lhs_is_id() && rhs_is_id();
  }

  bool lhs_is_id() {
    if (lhs->type == AST_EXPR_ID) return true;
    if (lhs->type != AST_EXPR_LHS) return false;
    return static_cast<LeftHandSideExpr *>(lhs)->is_id();
  }

  bool rhs_is_id() {
    if (rhs->type == AST_EXPR_ID) return true;
    if (rhs->type != AST_EXPR_LHS) return false;
    return static_cast<LeftHandSideExpr *>(rhs)->is_id();
  }

  bool rhs_is_1() {
    return rhs->is(ASTNode::AST_EXPR_NUMBER) && (rhs->as_number_literal()->num_val == 1.0);
  }

  optional<int8_t > rhs_as_int8() {
    if (!rhs->is(ASTNode::AST_EXPR_NUMBER)) return false;
    double val = rhs->as_number_literal()->num_val;
    if (val >= -128.0 && val <= 127.0 && val == (int)val) {
      return int8_t(val);
    } else {
      return std::nullopt;
    }
  }

  Token::TokenType assign_type;
  ASTNode *lhs;
  ASTNode *rhs;
};

class ProgramOrFunctionBody;

class Function : public ASTNode {
 public:

  Function(const Token& name, vector<u16string_view> params, ASTNode *body,
           u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_FUNC, source, start, end, line_start), name(name), params(std::move(params)) {
    assert(body->type == ASTNode::AST_FUNC_BODY);
    this->body = body;
    add_child(body);
  }

  ~Function() override { delete body; }

  std::string description() override {
    std::string desc = ASTNode::description() + " name: " + name.get_text_utf8();
    desc += ", params: ";
    for (auto& param : params) desc += to_u8string(param) + ", ";
    return desc;
  }

  bool has_name() { return name.type != Token::NONE; }

  u16string get_name_str() { return u16string(name.text); }

  Token name;
  vector<u16string_view> params;
  ASTNode *body;

  bool is_stmt {false};
  bool is_arrow_func {false};
};

class VarDecl : public ASTNode {
 public:
  VarDecl(const Token& id, u16string_view source, u32 start, u32 end, u32 line_start)
      : VarDecl(id, nullptr, source, start, end, line_start) {}

  VarDecl(const Token& id, ASTNode *var_init, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_VAR_DECL, source, start, end, line_start), id(id), var_init(var_init) {
    add_child(var_init);
  }
  ~VarDecl() override { delete var_init; }

  Token id;
  ASTNode *var_init;
};

class ProgramOrFunctionBody : public ASTNode {
 public:
  ProgramOrFunctionBody(Type type, bool strict) : ASTNode(type), strict(strict) {}

  ~ProgramOrFunctionBody() override {
    for (auto func_decl : func_decls) delete func_decl;
    for (auto stmt : statements) delete stmt;
  }

  void add_statement(ASTNode *stmt) {
    if (stmt->type == ASTNode::AST_FUNC && stmt->as_function()->is_stmt) {
      func_decls.push_back(stmt->as_function());
    } else {
      statements.push_back(stmt);
    }
    add_child(stmt);
  }

  bool strict;
  vector<ASTNode *> statements;
  vector<Function *> func_decls;
  unique_ptr<Scope> scope;
};

class LabelledStatement : public ASTNode {
 public:
  LabelledStatement(const Token& label, ASTNode *stmt, u16string_view source,
                    u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_LABEL, source, start, end, line_start),
        label(label), statement(stmt) {
    add_child(statement);
  }

  ~LabelledStatement() override { delete statement; }

  Token label;
  ASTNode *statement;
};

class ContinueOrBreak : public ASTNode {
 public:
  ContinueOrBreak(Type type, u16string_view source, u32 start, u32 end, u32 line_start)
      : ContinueOrBreak(type, Token::none, source, start, end, line_start) {}

  ContinueOrBreak(Type type, const Token& id, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(type, source, start, end, line_start), id(id) {}

  Token id;
};

class ReturnStatement : public ASTNode {
 public:
  ReturnStatement(ASTNode *expr, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_RETURN, source, start, end, line_start), expr(expr) {
    add_child(expr);
  }

  ~ReturnStatement() override {
    delete expr;
  }

  ASTNode *expr;
};

class ThrowStatement : public ASTNode {
 public:
  ThrowStatement(ASTNode *expr, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_THROW, source, start, end, line_start), expr(expr) {}

  ~ThrowStatement() override {
    delete expr;
  }

  ASTNode *expr;
};

class VarStatement : public ASTNode {
 public:

  explicit VarStatement(VarKind var_kind) : ASTNode(AST_STMT_VAR), kind(var_kind) {}
  ~VarStatement() override {
    for (auto decl : declarations) delete decl;
  }

  std::string description() override {
    return ASTNode::description() + "  " + get_var_kind_str(kind);
  }

  void add_decl(ASTNode *decl) {
    assert(decl->type == AST_STMT_VAR_DECL);
    declarations.emplace_back(static_cast<VarDecl *>(decl));
    add_child(decl);
  }

  bool is_lexical() {
    return kind == VarKind::DECL_LET || kind == VarKind::DECL_CONST;
  }

  VarKind kind;
  vector<VarDecl *> declarations;
};

class Block : public ASTNode {
 public:
  Block() : ASTNode(AST_STMT_BLOCK) {}

  ~Block() override {
    for (auto stmt : statements) { delete stmt; }
    for (auto stmt : func_decls) { delete stmt; }
  }

  void add_statement(ASTNode *stmt) {
    if (stmt->type == ASTNode::AST_FUNC && stmt->as_function()->is_stmt) {
      func_decls.push_back(stmt->as_function());
    } else {
      statements.push_back(stmt);
    }
    add_child(stmt);
  }

  vector<ASTNode *> statements;
  vector<Function *> func_decls;
  unique_ptr<Scope> scope;
};

class TryStatement : public ASTNode {
 public:
  TryStatement(ASTNode *try_block, const Token& catch_ident, ASTNode *catch_block, u16string_view source,
               u32 start, u32 end, u32 line_start)
      : TryStatement(try_block, catch_ident, catch_block, nullptr, source, start, end, line_start) {}

  TryStatement(ASTNode *try_block, ASTNode *finally_block, u16string_view source, u32 start,
               u32 end, u32 line_start)
      : TryStatement(try_block, Token::none, nullptr, finally_block, source, start,
                     end, line_start) {}

  TryStatement(ASTNode *try_block, const Token& catch_ident, ASTNode *catch_block, ASTNode *finally_block,
               u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_TRY, source, start, end, line_start), try_block(try_block),
        catch_ident(catch_ident), catch_block(catch_block), finally_block(finally_block) {

    add_child(try_block);
    add_child(catch_block);
    add_child(finally_block);
  }

  ~TryStatement() override {
    delete try_block;
    delete catch_block;
    delete finally_block;
  }

  u16string_view get_catch_identifier() { return catch_ident.text; };

  ASTNode *try_block;
  Token catch_ident;
  ASTNode *catch_block;
  ASTNode *finally_block;
};

class IfStatement : public ASTNode {
 public:
  IfStatement(ASTNode *cond, ASTNode *then_block, u16string_view source, u32 start, u32 end,
              u32 line_start)
      : IfStatement(cond, then_block, nullptr, source, start, end, line_start) {}

  IfStatement(ASTNode *cond, ASTNode *then_block, ASTNode *else_block, u16string_view source,
              u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_IF, source, start, end, line_start), condition_expr(cond), then_block(then_block),
        else_block(else_block) {

    add_child(cond);
    add_child(then_block);
    add_child(else_block);
  }

  ~IfStatement() override {
    delete condition_expr;
    delete then_block;
    delete else_block;
  }

  ASTNode *condition_expr;
  ASTNode *then_block;
  ASTNode *else_block;
};

class WhileStatement : public ASTNode {
 public:
  WhileStatement(ASTNode *condition_expr, ASTNode *body_stmt, u16string_view source, u32 start,
                 u32 end, u32 line_start)
      : ASTNode(AST_STMT_WHILE, source, start, end, line_start), condition_expr(condition_expr),
        body_stmt(body_stmt) {}

  ~WhileStatement() override {
    delete condition_expr;
    delete body_stmt;
  }

  ASTNode *condition_expr;
  ASTNode *body_stmt;
};

class WithStatement : public ASTNode {
 public:
  WithStatement(ASTNode *expr, ASTNode *stmt, u16string_view source, u32 start, u32 end,
                u32 line_start)
      : ASTNode(AST_STMT_WITH, source, start, end, line_start), expr(expr), stmt(stmt) {}

  ~WithStatement() override {
    delete expr;
    delete stmt;
  }

  ASTNode *expr;
  ASTNode *stmt;
};

class DoWhileStatement : public ASTNode {
 public:
  DoWhileStatement(ASTNode *condition_expr, ASTNode *body_stmt, u16string_view source,
                   u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_DO_WHILE, source, start, end, line_start),
        condition_expr(condition_expr), body_stmt(body_stmt) {}

  ~DoWhileStatement() override {
    delete condition_expr;
    delete body_stmt;
  }

  ASTNode *condition_expr;
  ASTNode *body_stmt;
};

class SwitchStatement : public ASTNode {
 public:
  struct CaseClause {
    CaseClause(ASTNode *expr, vector<ASTNode *> stmts)
        : expr(expr), stmts(std::move(stmts)) {}

    ASTNode *expr;
    u32 jump_point;
    vector<ASTNode *> stmts;
  };

  SwitchStatement() : ASTNode(AST_STMT_SWITCH) {}

  ~SwitchStatement() override {
    delete condition_expr;
    for (auto& clause : cases) {
      delete clause.expr;
      for (auto stmt : clause.stmts) { delete stmt; }
    }
    for (auto stmt : default_stmts) { delete stmt; }
  }

  void add_statement(ASTNode *stmt) {
    if (stmt->type == ASTNode::AST_FUNC && stmt->as_function()->is_stmt) {
      func_decls.push_back(stmt->as_function());
    } else {
      statements.push_back(stmt);
      if (stmt->is(ASTNode::AST_STMT_VAR) && stmt->as<VarStatement>()->is_lexical()) {
        lexical_var_def.push_back(stmt->as<VarStatement>());
      }
    }
    add_child(stmt);
  }

  ASTNode *condition_expr;
  bool has_default = false;
  // the index of the case right after the `default` clause.
  u32 default_index;
  vector<ASTNode *> default_stmts;
  vector<CaseClause> cases;
  unique_ptr<Scope> scope;
  vector<VarStatement *> lexical_var_def;
  vector<ASTNode *> statements;
  vector<Function *> func_decls;
};

class ForStatement : public ASTNode {
 public:
  ForStatement(ASTNode * init_expr, ASTNode *condition_expr, ASTNode *increment_expr,
               ASTNode *body_stmt, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_FOR, source, start, end, line_start), init_expr(init_expr),
        condition_expr(condition_expr), increment_expr(increment_expr), body_stmt(body_stmt) {
    add_child(init_expr);
    add_child(condition_expr);
    add_child(increment_expr);
    add_child(body_stmt);
  }

  ~ForStatement() override {
    delete init_expr;
    delete condition_expr;
    delete increment_expr;
    delete body_stmt;
  }

  ASTNode * init_expr;
  ASTNode *condition_expr;
  ASTNode *increment_expr;
  ASTNode *body_stmt;
};

class ForInStatement : public ASTNode {
 public:
  enum Type {
    FOR_IN,
    FOR_OF
  } iter_type;

  ForInStatement(ForInStatement::Type type, ASTNode *element_expr, ASTNode *collection_expr,
                 ASTNode *body_stmt, u16string_view source, u32 start, u32 end, u32 line_start)
      : ASTNode(AST_STMT_FOR_IN, source, start, end, line_start),
        iter_type(type),
        element_expr(element_expr),
        collection_expr(collection_expr),
        body_stmt(body_stmt) {
    add_child(element_expr);
    add_child(collection_expr);
    add_child(body_stmt);
  }

  bool element_is_id() {
    if (element_expr->is(AST_STMT_VAR)) return true;
    if (element_expr->is(AST_EXPR_ID)) return true;
    assert(element_expr->is(AST_EXPR_LHS));
    return static_cast<LeftHandSideExpr *>(element_expr)->is_id();
  }

  u16string_view get_element_id() {
    if (element_expr->is(AST_STMT_VAR)) {
      VarDecl *decl = element_expr->as<VarStatement>()->declarations[0];
      return decl->id.text;
    } else {
      assert(element_expr->is(AST_EXPR_LHS) || element_expr->is(AST_EXPR_ID));
      return element_expr->get_source();
    }
  }

  ~ForInStatement() override {
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