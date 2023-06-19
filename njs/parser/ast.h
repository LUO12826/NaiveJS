#ifndef NJS_PARSER_AST_H
#define NJS_PARSER_AST_H

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <utility>

#include "token.h"
#include "njs/utils/macros.h"

namespace njs {

class ASTNode {
 public:
  enum Type {
    AST_EXPR_THIS,
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
  ASTNode(Type type, std::u16string_view source, u32 start, u32 end) :
    type(type), text(source), start(start), end(end) {}
  virtual ~ASTNode() {};

  Type get_type() { return type; }
  std::u16string_view source() { return text; }
  const std::u16string_view& source_ref() { return text; }
  u32 start_pos() { return start; }
  u32 end_pos() { return end; }

  void set_source(const std::u16string_view& source, u32 start, u32 end) {
    text = source;
    start = start;
    end = end;
  }

  bool is_illegal() { return type == AST_ILLEGAL; }

  std::u16string get_label() { return label; }
  void set_label(std::u16string label) { label = label; }

 private:
  Type type;
  std::u16string_view text;
  u32 start;
  u32 end;
  std::u16string label;
};

class RegExpLiteral : public ASTNode {
 public:
  RegExpLiteral(std::u16string pattern, std::u16string flag,
                std::u16string_view source, u32 start, u32 end) :
    ASTNode(AST_EXPR_REGEXP, source, start, end), pattern(pattern), flag(flag) {}

  std::u16string get_pattern() { return pattern; }
  std::u16string get_flag() { return flag; }

 private:
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

  void set_property(Property p) {
    properties.emplace_back(p);
  }

  std::vector<Property> get_properties() { return properties; }

  u32 length() { return properties.size(); }

 private:
  std::vector<Property> properties;
};

class ParenthesisExpr : public ASTNode {
 public:
  ParenthesisExpr(ASTNode* expr, std::u16string_view source, u32 start, u32 end) :
    ASTNode(AST_EXPR_PAREN, source, start, end), expr(expr) {}

  ASTNode* get_expr() { return expr; }

 private:
  ASTNode* expr;
};

class BinaryExpr : public ASTNode {
 public:
  BinaryExpr(ASTNode* lhs, ASTNode* rhs, Token op, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_EXPR_BINARY, source, start, end), lhs(lhs), rhs(rhs), op(op) {}

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
  //          std::u16string source, u32 start, u32 end) :
  //   Function(Token::none, params, body, source, start, end) {}

  Function(Token name, std::vector<std::u16string> params, ASTNode* body,
           std::u16string source, u32 start, u32 end) :
    ASTNode(AST_FUNC, source, start, end), name_(name), params_(params) {
      ASSERT(body->get_type() == ASTNode::AST_FUNC_BODY);
      body_ = body;
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

class VarDecl;
class ProgramOrFunctionBody : public ASTNode {
 public:
  ProgramOrFunctionBody(Type type, bool strict) : ASTNode(type), strict_(strict) {}
  ~ProgramOrFunctionBody() override {
    for (auto func_decl : func_decls_)
      delete func_decl;
    for (auto stmt : stmts_)
      delete stmt;
  }

  void AddFunctionDecl(ASTNode* func) {
    ASSERT(func->get_type() == AST_FUNC);
    func_decls_.emplace_back(static_cast<Function*>(func));
  }
  void AddStatement(ASTNode* stmt) {
    stmts_.emplace_back(stmt);
  }

  bool strict() { return strict_; }
  std::vector<Function*> func_decls() { return func_decls_; }
  std::vector<ASTNode*> statements() { return stmts_; }

  std::vector<VarDecl*>& var_decls() { return var_decls_; }
  void SetVarDecls(std::vector<VarDecl*>&& var_decls) { var_decls_ = var_decls; }

 private:
  bool strict_;
  std::vector<Function*> func_decls_;
  std::vector<ASTNode*> stmts_;

  std::vector<VarDecl*> var_decls_;
};

class LabelledStatement : public ASTNode {
 public:
  LabelledStatement(Token label, ASTNode* stmt, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_LABEL, source, start, end), label_(label), stmt_(stmt) {}
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
  ContinueOrBreak(Type type, std::u16string source, u32 start, u32 end) :
    ContinueOrBreak(type, Token(TokenType::NONE, u"", 0, 0), source, start, end) {}

  ContinueOrBreak(Type type, Token ident, std::u16string source, u32 start, u32 end) :
    ASTNode(type, source, start, end), ident_(ident) {}

  std::u16string_view ident() { return ident_.text; }

 private:
  Token ident_;
};

class ReturnStatement : public ASTNode {
 public:
  ReturnStatement(ASTNode* expr, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_RETURN, source, start, end), expr_(expr) {}
  ~ReturnStatement() {
    if (expr_ != nullptr)
      delete expr_;
  }

  ASTNode* expr() { return expr_; }

 private:
  ASTNode* expr_;
};

class ThrowStatement : public ASTNode {
 public:
  ThrowStatement(ASTNode* expr, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_THROW, source, start, end), expr_(expr) {}
  ~ThrowStatement() {
    if (expr_ != nullptr)
      delete expr_;
  }

  ASTNode* expr() { return expr_; }

 private:
  ASTNode* expr_;
};

class VarDecl : public ASTNode {
 public:
  VarDecl(Token ident, std::u16string source, u32 start, u32 end) :
    VarDecl(ident, nullptr, source, start, end) {}

  VarDecl(Token ident, ASTNode* init, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_VAR_DECL, source, start, end), ident_(ident), init_(init) {}
  ~VarDecl() { delete init_; }

  Token& ident() { return ident_; }
  ASTNode* init() { return init_; }

 private:
  Token ident_;
  ASTNode* init_;
};

class VarStatement : public ASTNode {
 public:
  VarStatement() : ASTNode(AST_STMT_VAR) {}
  ~VarStatement() {
    for (auto decl : decls_)
      delete decl;
  }

  void AddDecl(ASTNode* decl) {
    ASSERT(decl->get_type() == AST_STMT_VAR_DECL);
    decls_.emplace_back(static_cast<VarDecl*>(decl));
  }

  std::vector<VarDecl*> decls() { return decls_; }

 public:
  std::vector<VarDecl*> decls_;
};

class Block : public ASTNode {
 public:
  Block() : ASTNode(AST_STMT_BLOCK) {}
  ~Block() {
    for (auto stmt : stmts_)
      delete stmt;
  }

  void AddStatement(ASTNode* stmt) {
    stmts_.emplace_back(stmt);
  }

  std::vector<ASTNode*> statements() { return stmts_; }

 public:
  std::vector<ASTNode*> stmts_;
};

class TryStatement : public ASTNode {
 public:
  TryStatement(ASTNode* try_block, Token catch_ident, ASTNode* catch_block,
      std::u16string source, u32 start, u32 end) :
    TryStatement(try_block, catch_ident, catch_block, nullptr, source, start, end) {}

  TryStatement(ASTNode* try_block, ASTNode* finally_block,
      std::u16string source, u32 start, u32 end) :
    TryStatement(try_block, Token(TokenType::NONE, u"", 0, 0), nullptr, finally_block, source, start, end) {}

  TryStatement(ASTNode* try_block, Token catch_ident, ASTNode* catch_block, ASTNode* finally_block,
      std::u16string source, u32 start, u32 end)
    : ASTNode(AST_STMT_TRY, source, start, end), try_block_(try_block), catch_ident_(catch_ident),
      catch_block_(catch_block), finally_block_(finally_block) {}

  ~TryStatement() {
    delete try_block_;
    if (catch_block_ != nullptr)
      delete catch_block_;
    if (finally_block_ != nullptr)
      delete finally_block_;
  }

  ASTNode* try_block() { return try_block_; }
  std::u16string_view catch_ident() { return catch_ident_.text; };
  ASTNode* catch_block() { return catch_block_; }
  ASTNode* finally_block() { return finally_block_; }

 public:
  ASTNode* try_block_;
  Token catch_ident_;
  ASTNode* catch_block_;
  ASTNode* finally_block_;
};

class IfStatement : public ASTNode {
 public:
  IfStatement(ASTNode* cond, ASTNode* if_block, std::u16string source, u32 start, u32 end) :
    IfStatement(cond, if_block, nullptr, source, start, end) {}

  IfStatement(ASTNode* cond, ASTNode* if_block, ASTNode* else_block, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_IF, source, start, end), cond_(cond), if_block_(if_block), else_block_(else_block) {}
  ~IfStatement() {
    delete cond_;
    delete if_block_;
    if (else_block_ != nullptr)
      delete else_block_;
  }

  ASTNode* cond() { return cond_; }
  ASTNode* if_block() { return if_block_; }
  ASTNode* else_block() { return else_block_; }

 public:
  ASTNode* cond_;
  ASTNode* if_block_;
  ASTNode* else_block_;
};

class WhileOrWith : public ASTNode {
 public:
  WhileOrWith(Type type, ASTNode* expr, ASTNode* stmt,
              std::u16string source, u32 start, u32 end) :
    ASTNode(type, source, start, end), expr_(expr), stmt_(stmt) {}
  ~WhileOrWith() {
    delete expr_;
    delete stmt_;
  }

  ASTNode* expr() { return expr_; }
  ASTNode* stmt() { return stmt_; }

 public:
  ASTNode* expr_;
  ASTNode* stmt_;
};

class DoWhileStatement : public ASTNode {
 public:
  DoWhileStatement(ASTNode* expr, ASTNode* stmt, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_DO_WHILE, source, start, end), expr_(expr), stmt_(stmt) {}
  ~DoWhileStatement() {
    delete expr_;
    delete stmt_;
  }

  ASTNode* expr() { return expr_; }
  ASTNode* stmt() { return stmt_; }

 public:
  ASTNode* expr_;
  ASTNode* stmt_;
};

class SwitchStatement : public ASTNode {
 public:
  SwitchStatement() : ASTNode(AST_STMT_SWITCH) {}

  ~SwitchStatement() override {
    for (CaseClause clause : before_default_case_clauses_) {
      delete clause.expr;
      for (auto stmt : clause.stmts) {
        delete stmt;
      }
    }
    for (CaseClause clause : after_default_case_clauses_) {
      delete clause.expr;
      for (auto stmt : clause.stmts) {
        delete stmt;
      }
    }
    for (auto stmt : default_clause_.stmts) {
      delete  stmt;
    }
  }

  void SetExpr(ASTNode* expr) {
    expr_ = expr;
  }

  struct DefaultClause {
    std::vector<ASTNode*> stmts;
  };

  struct CaseClause {
    CaseClause(ASTNode* expr, std::vector<ASTNode*> stmts) : expr(expr), stmts(stmts) {}
    ASTNode* expr;
    std::vector<ASTNode*> stmts;
  };

  void SetDefaultClause(std::vector<ASTNode*> stmts) {
    ASSERT(!has_default_clause());
    has_default_clause_ = true;
    default_clause_.stmts = stmts;
  }

  void AddBeforeDefaultCaseClause(CaseClause c) {
    before_default_case_clauses_.emplace_back(c);
  }

  void AddAfterDefaultCaseClause(CaseClause c) {
    after_default_case_clauses_.emplace_back(c);
  }

  ASTNode* expr() { return expr_; }
  std::vector<CaseClause> before_default_case_clauses() { return before_default_case_clauses_; }
  bool has_default_clause() { return has_default_clause_; }
  DefaultClause default_clause() {
    ASSERT(has_default_clause());
    return default_clause_;
  }
  std::vector<CaseClause> after_default_case_clauses() { return after_default_case_clauses_; }

 private:
  ASTNode* expr_;
  bool has_default_clause_ = false;
  DefaultClause default_clause_;
  std::vector<CaseClause> before_default_case_clauses_;
  std::vector<CaseClause> after_default_case_clauses_;
};

class ForStatement : public ASTNode {
 public:
  ForStatement(std::vector<ASTNode*> expr0s, ASTNode* expr1, ASTNode* expr2, ASTNode* stmt,
      std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_FOR, source, start, end), expr0s_(expr0s), expr1_(expr1), expr2_(expr2), stmt_(stmt) {}

  std::vector<ASTNode*> expr0s() { return expr0s_; }
  ASTNode* expr1() { return expr1_; }
  ASTNode* expr2() { return expr2_; }
  ASTNode* statement() { return stmt_; }

 private:
  std::vector<ASTNode*> expr0s_;
  ASTNode* expr1_;
  ASTNode* expr2_;

  ASTNode* stmt_;
};

class ForInStatement : public ASTNode {
 public:
  ForInStatement(ASTNode* expr0, ASTNode* expr1, ASTNode* stmt,
        std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_FOR_IN, source, start, end), expr0_(expr0), expr1_(expr1), stmt_(stmt) {}

  ASTNode* expr0() { return expr0_; }
  ASTNode* expr1() { return expr1_; }
  ASTNode* statement() { return stmt_; }

 private:
  ASTNode* expr0_;
  ASTNode* expr1_;

  ASTNode* stmt_;
};

}  // namespace njs

#endif  // NJS_PARSER_AST_H