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

  ASTNode(Type type) : type_(type) {}
  ASTNode(Type type, std::u16string_view source, u32 start, u32 end) :
    type_(type), source_(source), start_(start), end_(end) {}
  virtual ~ASTNode() {};

  Type type() { return type_; }
  std::u16string_view source() { return source_; }
  const std::u16string_view& source_ref() { return source_; }
  u32 start() { return start_; }
  u32 end() { return end_; }

  void SetSource(std::u16string source, u32 start, u32 end) {
    source_ = source;
    start_ = start;
    end_ = end;
  }

  bool is_illegal() { return type_ == AST_ILLEGAL; }

  std::u16string label() { return label_; }
  void SetLabel(std::u16string label) { label_ = label; }

 private:
  Type type_;
  std::u16string_view source_;
  u32 start_;
  u32 end_;
  std::u16string label_;
};

class RegExpLiteral : public ASTNode {
 public:
  RegExpLiteral(std::u16string pattern, std::u16string flag,
                std::u16string_view source, u32 start, u32 end) :
    ASTNode(AST_EXPR_REGEXP, source, start, end), pattern_(pattern), flag_(flag) {}

  std::u16string pattern() { return pattern_; }
  std::u16string flag() { return flag_; }

 private:
  std::u16string pattern_;
  std::u16string flag_;
};

class ArrayLiteral : public ASTNode {
 public:
  ArrayLiteral() : ASTNode(AST_EXPR_ARRAY), len_(0) {}

  ~ArrayLiteral() override {
    for (auto pair : elements_) {
      delete pair.second;
    }
  }

  u32 length() { return len_; }
  std::vector<std::pair<u32, ASTNode*>> elements() { return elements_; }

  void AddElement(ASTNode* element) {
    if (element != nullptr) {
      elements_.emplace_back(len_, element);
    }
    len_++;
  }

 private:
  std::vector<std::pair<u32, ASTNode*>> elements_;
  u32 len_;
};

class ObjectLiteral : public ASTNode {
 public:
  ObjectLiteral() : ASTNode(AST_EXPR_OBJ) {}

  ~ObjectLiteral() override {
    for (auto property : properties_) {
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

  void AddProperty(Property p) {
    properties_.emplace_back(p);
  }

  std::vector<Property> properties() { return properties_; }

  u32 length() { return properties_.size(); }

 private:
  std::vector<Property> properties_;
};

class Paren : public ASTNode {
 public:
  Paren(ASTNode* expr, std::u16string_view source, u32 start, u32 end) :
    ASTNode(AST_EXPR_PAREN, source, start, end), expr_(expr) {}

  ASTNode* expr() { return expr_; }

 private:
  ASTNode* expr_;
};

class Binary : public ASTNode {
 public:
  Binary(ASTNode* lhs, ASTNode* rhs, Token op, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_EXPR_BINARY, source, start, end), lhs_(lhs), rhs_(rhs), op_(op) {}

  ~Binary() override {
    delete lhs_;
    delete rhs_;
  }

  ASTNode* lhs() { return lhs_; }
  ASTNode* rhs() { return rhs_; }
  Token& op() { return op_; }

 private:
  ASTNode* lhs_;
  ASTNode* rhs_;
  Token op_;
};

class Unary : public ASTNode {
 public:
  Unary(ASTNode* node, Token op, bool prefix) :
    ASTNode(AST_EXPR_UNARY), node_(node), op_(op), prefix_(prefix) {}

  ~Unary() override {
    delete node_;
  }

  ASTNode* node() { return node_; }
  Token& op() { return op_; }
  bool prefix() { return prefix_; }

 private:
  ASTNode* node_;
  Token op_;
  bool prefix_;
};

class TripleCondition : public ASTNode {
 public:
  TripleCondition(ASTNode* cond, ASTNode* true_expr, ASTNode* false_expr) :
    ASTNode(AST_EXPR_TRIPLE), cond_(cond), true_expr_(true_expr), false_expr_(false_expr) {}

  ~TripleCondition() override {
    delete cond_;
    delete true_expr_;
    delete false_expr_;
  }

  ASTNode* cond() { return cond_; }
  ASTNode* true_expr() { return true_expr_; }
  ASTNode* false_expr() { return false_expr_; }

 private:
  ASTNode* cond_;
  ASTNode* true_expr_;
  ASTNode* false_expr_;
};

class Expression : public ASTNode {
 public:
  Expression() : ASTNode(AST_EXPR) {}
  ~Expression() override {
    for (auto element : elements_) {
      delete element;
    }
  }

  void AddElement(ASTNode* element) { elements_.push_back(element); }

  std::vector<ASTNode*> elements() { return elements_; }

 private:
  std::vector<ASTNode*> elements_;
};

class Arguments : public ASTNode {
 public:
  Arguments(std::vector<ASTNode*> args) : ASTNode(AST_EXPR_ARGS), args_(args) {}

  ~Arguments() override {
    for (auto arg : args_)
      delete arg;
  }

  std::vector<ASTNode*> args() { return args_; }

 private:
  std::vector<ASTNode*> args_;
};

class LHS : public ASTNode {
 public:
  LHS(ASTNode* base, u32 new_count) :
    ASTNode(AST_EXPR_LHS), base_(base), new_count_(new_count) {}

  ~LHS() override {
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
      ASSERT(body->type() == ASTNode::AST_FUNC_BODY);
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
    ASSERT(func->type() == AST_FUNC);
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

class LabelledStmt : public ASTNode {
 public:
  LabelledStmt(Token label, ASTNode* stmt, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_LABEL, source, start, end), label_(label), stmt_(stmt) {}
  ~LabelledStmt() {
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

class Return : public ASTNode {
 public:
  Return(ASTNode* expr, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_RETURN, source, start, end), expr_(expr) {}
  ~Return() {
    if (expr_ != nullptr)
      delete expr_;
  }

  ASTNode* expr() { return expr_; }

 private:
  ASTNode* expr_;
};

class Throw : public ASTNode {
 public:
  Throw(ASTNode* expr, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_THROW, source, start, end), expr_(expr) {}
  ~Throw() {
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

class VarStmt : public ASTNode {
 public:
  VarStmt() : ASTNode(AST_STMT_VAR) {}
  ~VarStmt() {
    for (auto decl : decls_)
      delete decl;
  }

  void AddDecl(ASTNode* decl) {
    ASSERT(decl->type() == AST_STMT_VAR_DECL);
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

class Try : public ASTNode {
 public:
  Try(ASTNode* try_block, Token catch_ident, ASTNode* catch_block,
      std::u16string source, u32 start, u32 end) :
    Try(try_block, catch_ident, catch_block, nullptr, source, start, end) {}

  Try(ASTNode* try_block, ASTNode* finally_block,
      std::u16string source, u32 start, u32 end) :
    Try(try_block, Token(TokenType::NONE, u"", 0, 0), nullptr, finally_block, source, start, end) {}

  Try(ASTNode* try_block, Token catch_ident, ASTNode* catch_block, ASTNode* finally_block,
      std::u16string source, u32 start, u32 end)
    : ASTNode(AST_STMT_TRY, source, start, end), try_block_(try_block), catch_ident_(catch_ident),
      catch_block_(catch_block), finally_block_(finally_block) {}

  ~Try() {
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

class If : public ASTNode {
 public:
  If(ASTNode* cond, ASTNode* if_block, std::u16string source, u32 start, u32 end) :
    If(cond, if_block, nullptr, source, start, end) {}

  If(ASTNode* cond, ASTNode* if_block, ASTNode* else_block, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_IF, source, start, end), cond_(cond), if_block_(if_block), else_block_(else_block) {}
  ~If() {
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

class DoWhile : public ASTNode {
 public:
  DoWhile(ASTNode* expr, ASTNode* stmt, std::u16string source, u32 start, u32 end) :
    ASTNode(AST_STMT_DO_WHILE, source, start, end), expr_(expr), stmt_(stmt) {}
  ~DoWhile() {
    delete expr_;
    delete stmt_;
  }

  ASTNode* expr() { return expr_; }
  ASTNode* stmt() { return stmt_; }

 public:
  ASTNode* expr_;
  ASTNode* stmt_;
};

class Switch : public ASTNode {
 public:
  Switch() : ASTNode(AST_STMT_SWITCH) {}

  ~Switch() override {
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

class For : public ASTNode {
 public:
  For(std::vector<ASTNode*> expr0s, ASTNode* expr1, ASTNode* expr2, ASTNode* stmt,
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

class ForIn : public ASTNode {
 public:
  ForIn(ASTNode* expr0, ASTNode* expr1, ASTNode* stmt,
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