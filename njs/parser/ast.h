#ifndef NJS_AST_H
#define NJS_AST_H

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdio>

#include "token.h"
#include "njs/utils/macros.h"

namespace njs {

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
  ASTNode(Type type, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    type(type), text(source), start(start), end(end), line_start(line_start) {}
  virtual ~ASTNode() {};

  Type get_type() { return type; }
  std::u16string_view source() { return text; }
  const std::u16string_view& source_ref() { return text; }
  u32 start_pos() { return start; }
  u32 end_pos() { return end; }
  u32 get_line_start() { return line_start; }

  void set_source(const std::u16string_view& source, u32 start, u32 end, u32 line_start) {
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

 private:
  Type type;
  std::u16string_view text;
  u32 start;
  u32 end;
  u32 line_start;
  std::vector<ASTNode*> children;
};

class RegExpLiteral : public ASTNode {
 public:
  RegExpLiteral(std::u16string pattern, std::u16string flag,
                std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_EXPR_REGEXP, source, start, end, line_start), pattern(pattern), flag(flag) {}

  std::u16string pattern;
  std::u16string flag;
};

class ArrayLiteral : public ASTNode {
 public:
  ArrayLiteral() : ASTNode(AST_EXPR_ARRAY), len(0) {}

  ~ArrayLiteral() override {
    for (auto pair : elements) {
      delete pair.second;
    }
  }

  u32 get_length() { return len; }
  std::vector<std::pair<u32, ASTNode*>> get_elements() { return elements; }

  void add_element(ASTNode* element) {
    if (element != nullptr) {
      elements.emplace_back(len, element);
    }
    len++;
  }

 private:
  std::vector<std::pair<u32, ASTNode*>> elements;
  u32 len;
};

class ObjectLiteral : public ASTNode {
 public:
  ObjectLiteral() : ASTNode(AST_EXPR_OBJ) {}

  ~ObjectLiteral() override {
    for (auto property : properties) {
      delete property.value;
    }
  }

  struct Property {
    enum Type {
      NORMAL = 0,
      GET,
      SET,
    };

    Property(Token k, ASTNode* v, Type t) : key(k), value(v), type(t) {}

    Token key;
    ASTNode* value;
    Type type;
  };

  void set_property(const Property& p) {
    properties.emplace_back(p);
  }

  std::vector<Property> get_properties() { return properties; }

  u32 length() { return properties.size(); }

 private:
  std::vector<Property> properties;
};

class ParenthesisExpr : public ASTNode {
 public:
  ParenthesisExpr(ASTNode* expr, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_EXPR_PAREN, source, start, end, line_start), expr(expr) {}

  ASTNode* get_expr() { return expr; }

 private:
  ASTNode* expr;
};

class BinaryExpr : public ASTNode {
 public:
  BinaryExpr(ASTNode* lhs, ASTNode* rhs, Token op, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_EXPR_BINARY, source, start, end, line_start), lhs(lhs), rhs(rhs), op(op) {}

  ~BinaryExpr() override {
    delete lhs;
    delete rhs;
  }

  ASTNode* get_lhs() { return lhs; }
  ASTNode* get_rhs() { return rhs; }
  Token& get_op() { return op; }

 private:
  ASTNode* lhs;
  ASTNode* rhs;
  Token op;
};

class UnaryExpr : public ASTNode {
 public:
  UnaryExpr(ASTNode* node, Token op, bool prefix) :
    ASTNode(AST_EXPR_UNARY), operand(node), op(op), prefix_op(prefix) {}

  ~UnaryExpr() override {
    delete operand;
  }

  ASTNode* node() { return operand; }
  Token& get_op() { return op; }
  bool is_prefix_op() { return prefix_op; }

 private:
  ASTNode* operand;
  Token op;
  bool prefix_op;
};

class TernaryExpr : public ASTNode {
 public:
  TernaryExpr(ASTNode* cond, ASTNode* true_expr, ASTNode* false_expr) :
    ASTNode(AST_EXPR_TRIPLE), cond_expr(cond), true_expr(true_expr), false_expr(false_expr) {}

  ~TernaryExpr() override {
    delete cond_expr;
    delete true_expr;
    delete false_expr;
  }

  ASTNode* get_cond() { return cond_expr; }
  ASTNode* get_true_expr() { return true_expr; }
  ASTNode* get_false_expr() { return false_expr; }

 private:
  ASTNode* cond_expr;
  ASTNode* true_expr;
  ASTNode* false_expr;
};

class Expression : public ASTNode {
 public:
  Expression() : ASTNode(AST_EXPR) {}
  ~Expression() override {
    for (auto element : elements) {
      delete element;
    }
  }

  void add_element(ASTNode* element) { elements.push_back(element); }

  std::vector<ASTNode*> get_elements() { return elements; }

 private:
  std::vector<ASTNode*> elements;
};

class Arguments : public ASTNode {
 public:
  Arguments(std::vector<ASTNode*> args) : ASTNode(AST_EXPR_ARGS), args(args) {}

  ~Arguments() override {
    for (auto arg : args)
      delete arg;
  }

  std::vector<ASTNode*> get_args() { return args; }

 private:
  std::vector<ASTNode*> args;
};

class LeftHandSideExpr : public ASTNode {
 public:
  LeftHandSideExpr(ASTNode* base, u32 new_count) :
    ASTNode(AST_EXPR_LHS), base_(base), new_count_(new_count) {}

  ~LeftHandSideExpr() override {
    for (auto args : args_list_)
      delete args;
    for (auto index : index_list_)
      delete index;
  }

  enum PostfixType{
    CALL,
    INDEX,
    PROP,
  };

  void AddArguments(Arguments* args) {
    order_.emplace_back(std::make_pair(args_list_.size(), CALL));
    args_list_.emplace_back(args);
  }

  void AddIndex(ASTNode* index) {
    order_.emplace_back(std::make_pair(index_list_.size(), INDEX));
    index_list_.emplace_back(index);
  }

  void AddProp(Token prop_name) {
    order_.emplace_back(std::make_pair(prop_name_list_.size(), PROP));
    prop_name_list_.emplace_back(prop_name.text);
  }

  ASTNode* base() { return base_; }
  u32 new_count() { return new_count_; }
  std::vector<std::pair<u32, PostfixType>> order() { return order_; }
  std::vector<Arguments*> args_list() { return args_list_; }
  std::vector<ASTNode*> index_list() { return index_list_; }
  std::vector<std::u16string> prop_name_list() { return prop_name_list_; }

 private:
  ASTNode* base_;
  u32 new_count_;

  std::vector<std::pair<u32, PostfixType>> order_;
  std::vector<Arguments*> args_list_;
  std::vector<ASTNode*> index_list_;
  std::vector<std::u16string> prop_name_list_;
};

class Function : public ASTNode {
 public:
  // Function(std::vector<std::u16string> params, AST* body,
  //          std::u16string_view source, u32 start, u32 end) :
  //   Function(Token::none, params, body, source, start, end, line_start) {}

  Function(Token name, std::vector<std::u16string> params, ASTNode* body,
           std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_FUNC, source, start, end, line_start), name_(name), params_(params) {
      ASSERT(body->get_type() == ASTNode::AST_FUNC_BODY);
      body_ = body;
      add_child(body);
    }

  ~Function() override {
    delete body_;
  }

  bool is_named() { return name_.type != TokenType::NONE; }
  std::u16string_view name() { return name_.text; }
  std::vector<std::u16string>& params() { return params_; }
  ASTNode* body() { return body_; }

 private:
  Token name_;
  std::vector<std::u16string> params_;
  ASTNode* body_;
};

class VarDecl : public ASTNode {
 public:
  VarDecl(Token ident, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    VarDecl(ident, nullptr, source, start, end, line_start) {}

  VarDecl(Token ident, ASTNode* init, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_STMT_VAR_DECL, source, start, end, line_start), ident_(ident), init_(init) {}
  ~VarDecl() { delete init_; }

  Token& ident() { return ident_; }
  ASTNode* init() { return init_; }

 private:
  Token ident_;
  ASTNode* init_;
};

class ProgramOrFunctionBody : public ASTNode {
 public:
  ProgramOrFunctionBody(Type type, bool strict) : ASTNode(type), strict_(strict) {}
  ~ProgramOrFunctionBody() override {
    for (auto func_decl : func_decls_)
      delete func_decl;
    for (auto stmt : stmts_)
      delete stmt;
  }

  void add_function_decl(ASTNode* func) {
    ASSERT(func->get_type() == AST_FUNC);
    func_decls_.emplace_back(static_cast<Function*>(func));
    add_child(func);
  }
  void add_statement(ASTNode* stmt) {
    stmts_.emplace_back(stmt);
    add_child(stmt);
  }

  bool strict() { return strict_; }
  std::vector<Function*> func_decls() { return func_decls_; }
  std::vector<ASTNode*> statements() { return stmts_; }

  std::vector<VarDecl*>& var_decls() { return var_decls_; }
  void set_var_decls(std::vector<VarDecl*>&& var_decls) { this->var_decls_ = var_decls; }

 private:
  bool strict_;
  std::vector<Function*> func_decls_;
  std::vector<ASTNode*> stmts_;

  std::vector<VarDecl*> var_decls_;
};

class LabelledStatement : public ASTNode {
 public:
  LabelledStatement(Token label, ASTNode* stmt, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_STMT_LABEL, source, start, end, line_start), label_(label), stmt_(stmt) {}
  ~LabelledStatement() {
    delete stmt_;
  }

  std::u16string_view label() { return label_.text; }
  ASTNode* statement() { return stmt_; }

 private:
  Token label_;
  ASTNode* stmt_;
};

class ContinueOrBreak : public ASTNode {
 public:
  ContinueOrBreak(Type type, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ContinueOrBreak(type, Token(TokenType::NONE, u"", 0, 0), source, start, end, line_start) {}

  ContinueOrBreak(Type type, Token ident, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(type, source, start, end, line_start), ident_(ident) {}

  std::u16string_view ident() { return ident_.text; }

 private:
  Token ident_;
};

class ReturnStatement : public ASTNode {
 public:
  ReturnStatement(ASTNode* expr, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_STMT_RETURN, source, start, end, line_start), expr(expr) {}
  ~ReturnStatement() {
    if (expr != nullptr)
      delete expr;
  }

  ASTNode* expr;
};

class ThrowStatement : public ASTNode {
 public:
  ThrowStatement(ASTNode* expr, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    ASTNode(AST_STMT_THROW, source, start, end, line_start), expr(expr) {}
  ~ThrowStatement() {
    if (expr != nullptr)
      delete expr;
  }

  ASTNode* expr;
};

class VarStatement : public ASTNode {
 public:
  VarStatement() : ASTNode(AST_STMT_VAR) {}
  ~VarStatement() {
    for (auto decl : declarations) delete decl;
  }

  void add_decl(ASTNode* decl) {
    ASSERT(decl->get_type() == AST_STMT_VAR_DECL);
    declarations.emplace_back(static_cast<VarDecl*>(decl));
  }

  std::vector<VarDecl*> declarations;
};

class Block : public ASTNode {
 public:
  Block() : ASTNode(AST_STMT_BLOCK) {}
  ~Block() {
    for (auto stmt : statements) {
      delete stmt;
    }
  }

  void add_statement(ASTNode* stmt) {
    statements.emplace_back(stmt);
    add_child(stmt);
  }

  std::vector<ASTNode*> statements;
};

class TryStatement : public ASTNode {
 public:
  TryStatement(ASTNode* try_block, Token catch_ident, ASTNode* catch_block,
      std::u16string_view source, u32 start, u32 end, u32 line_start) :
    TryStatement(try_block, catch_ident, catch_block, nullptr, source, start, end, line_start) {}

  TryStatement(ASTNode* try_block, ASTNode* finally_block,
      std::u16string_view source, u32 start, u32 end, u32 line_start) :
    TryStatement(try_block, Token(TokenType::NONE, u"", 0, 0),
                  nullptr, finally_block, source, start, end, line_start) {}

  TryStatement(ASTNode* try_block, Token catch_ident, ASTNode* catch_block, ASTNode* finally_block,
      std::u16string_view source, u32 start, u32 end, u32 line_start)
    : ASTNode(AST_STMT_TRY, source, start, end, line_start),
                try_block(try_block), catch_ident(catch_ident),
                catch_block(catch_block), finally_block(finally_block) {
    
    add_child(try_block);
    add_child(catch_block);
    add_child(finally_block);
  }

  ~TryStatement() {
    delete try_block;
    if (catch_block != nullptr) delete catch_block;
    if (finally_block != nullptr) delete finally_block;
  }

  std::u16string_view get_catch_identifier() { return catch_ident.text; };

  ASTNode* try_block;
  Token catch_ident;
  ASTNode* catch_block;
  ASTNode* finally_block;
};

class IfStatement : public ASTNode {
 public:
  IfStatement(ASTNode* cond, ASTNode* if_block, std::u16string_view source, u32 start, u32 end, u32 line_start) :
    IfStatement(cond, if_block, nullptr, source, start, end, line_start) {}

  IfStatement(ASTNode* cond, ASTNode* if_block, ASTNode* else_block,
              std::u16string_view source, u32 start, u32 end, u32 line_start) :
              ASTNode(AST_STMT_IF, source, start, end, line_start),
              condition_expr(cond), if_block(if_block), else_block(else_block) {
    
    add_child(cond);
    add_child(if_block);
    add_child(else_block);
  }

  ~IfStatement() {
    delete condition_expr;
    delete if_block;
    if (else_block != nullptr)
      delete else_block;
  }

  ASTNode* condition_expr;
  ASTNode* if_block;
  ASTNode* else_block;
};

class WhileStatement : public ASTNode {
 public:
  WhileStatement(ASTNode* condition_expr, ASTNode* body_stmt, std::u16string_view source,
                  u32 start, u32 end, u32 line_start) :
                  ASTNode(AST_STMT_WHILE, source, start, end, line_start),
                           condition_expr(condition_expr), body_stmt(body_stmt) {}

  ~WhileStatement() {
    delete condition_expr;
    delete body_stmt;
  }

  ASTNode* condition_expr;
  ASTNode* body_stmt;
};

class WithStatement : public ASTNode {
 public:
  WithStatement(ASTNode* expr, ASTNode* stmt, std::u16string_view source,
                u32 start, u32 end, u32 line_start) :
                ASTNode(AST_STMT_WITH, source, start, end, line_start), expr(expr), stmt(stmt) {}

  ~WithStatement() {
    delete expr;
    delete stmt;
  }

  ASTNode* expr;
  ASTNode* stmt;
};

class DoWhileStatement : public ASTNode {
 public:
  DoWhileStatement(ASTNode* condition_expr, ASTNode* body_stmt,
                  std::u16string_view source, u32 start, u32 end, u32 line_start) :
                  ASTNode(AST_STMT_DO_WHILE, source, start, end, line_start),
                  condition_expr(condition_expr), body_stmt(body_stmt) {}

  ~DoWhileStatement() {
    delete condition_expr;
    delete body_stmt;
  }

  ASTNode* condition_expr;
  ASTNode* body_stmt;
};

class SwitchStatement : public ASTNode {
 public:
  struct DefaultClause {
    std::vector<ASTNode*> stmts;
  };

  struct CaseClause {
    CaseClause(ASTNode* expr, std::vector<ASTNode*> stmts) : expr(expr), stmts(stmts) {}
    ASTNode* expr;
    std::vector<ASTNode*> stmts;
  };

  SwitchStatement() : ASTNode(AST_STMT_SWITCH) {}

  ~SwitchStatement() override {
    for (CaseClause clause : before_default_case_clauses) {
      delete clause.expr;
      for (auto stmt : clause.stmts) {
        delete stmt;
      }
    }
    for (CaseClause clause : after_default_case_clauses) {
      delete clause.expr;
      for (auto stmt : clause.stmts) {
        delete stmt;
      }
    }
    for (auto stmt : default_clause.stmts) {
      delete  stmt;
    }
  }

  void SetExpr(ASTNode* expr) {
    condition_expr = expr;
  }

  void SetDefaultClause(std::vector<ASTNode*> stmts) {
    ASSERT(!has_default_clause);
    has_default_clause = true;
    default_clause.stmts = stmts;
  }

  void AddBeforeDefaultCaseClause(CaseClause c) {
    before_default_case_clauses.emplace_back(c);
  }

  void AddAfterDefaultCaseClause(CaseClause c) {
    after_default_case_clauses.emplace_back(c);
  }

  ASTNode* condition_expr;
  bool has_default_clause = false;
  DefaultClause default_clause;
  std::vector<CaseClause> before_default_case_clauses;
  std::vector<CaseClause> after_default_case_clauses;
};

class ForStatement : public ASTNode {
 public:
  ForStatement(std::vector<ASTNode*> init_expr, ASTNode* condition_expr, ASTNode* increment_expr,
              ASTNode* body_stmt, std::u16string_view source, u32 start, u32 end, u32 line_start) :
              ASTNode(AST_STMT_FOR, source, start, end, line_start), init_expr(init_expr),
              condition_expr(condition_expr), increment_expr(increment_expr), body_stmt(body_stmt) {}

  std::vector<ASTNode*> init_expr;
  ASTNode* condition_expr;
  ASTNode* increment_expr;

  ASTNode* body_stmt;
};

class ForInStatement : public ASTNode {
 public:
  ForInStatement(ASTNode* element_expr, ASTNode* collection_expr, ASTNode* body_stmt,
                std::u16string_view source, u32 start, u32 end, u32 line_start) :
                ASTNode(AST_STMT_FOR_IN, source, start, end, line_start), element_expr(element_expr),
                collection_expr(collection_expr), body_stmt(body_stmt) {}

  ASTNode* element_expr;
  ASTNode* collection_expr;

  ASTNode* body_stmt;
};

}  // namespace njs

#endif  // NJS_AST_H