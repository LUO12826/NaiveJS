#ifndef NJS_PARSER_H
#define NJS_PARSER_H

#include <stack>
#include <iostream>

#include "njs/include/SmallVector.h"
#include "njs/parser/lexer.h"
#include "njs/parser/ast.h"
#include "njs/utils/helper.h"
#include "njs/codegen/SymbolTable.h"

#define START_POS u32 start = lexer.current().start, line_start = lexer.current().line
#define RENEW_START start = lexer.current().start; line_start = lexer.current().line
#define SOURCE_PARSED_EXPR std::u16string_view(source.data() + start, lexer.current_pos() - start), \
                            start, lexer.current_pos(), line_start

#define TOKEN_SOURCE_EXPR token.text, token.start, token.end, token.line

namespace njs {

using TokenType = Token::TokenType;

struct ParserContext {
};

class Parser {
 public:
  Parser(std::u16string source) : source(source), lexer(source) {}

  ASTNode* parse_primary_expression() {
    Token token = lexer.current();
    switch (token.type) {
      case TokenType::KEYWORD:
        if (token.text == u"this") {
          return new ASTNode(ASTNode::AST_EXPR_THIS, TOKEN_SOURCE_EXPR);
        }
        goto error;
      case TokenType::STRICT_FUTURE_KW:
        return new ASTNode(ASTNode::AST_EXPR_STRICT_FUTURE, TOKEN_SOURCE_EXPR);
      case TokenType::IDENTIFIER:
        return new ASTNode(ASTNode::AST_EXPR_ID, TOKEN_SOURCE_EXPR);
      case TokenType::TK_NULL:
        return new ASTNode(ASTNode::AST_EXPR_NULL, TOKEN_SOURCE_EXPR);
      case TokenType::TK_BOOL:
        return new ASTNode(ASTNode::AST_EXPR_BOOL, TOKEN_SOURCE_EXPR);
      case TokenType::NUMBER:
        return new ASTNode(ASTNode::AST_EXPR_NUMBER, TOKEN_SOURCE_EXPR);
      case TokenType::STRING:
        return new ASTNode(ASTNode::AST_EXPR_STRING, TOKEN_SOURCE_EXPR);
      case TokenType::LEFT_BRACK:  // [
        return parse_array_literal();
      case TokenType::LEFT_BRACE:  // {
        return parse_object_literal();
      case TokenType::LEFT_PAREN: { // (
        lexer.next();   // skip (
        ASTNode* value = parse_expression(false);
        if (value->is_illegal()) return value;
        if (lexer.next().type != TokenType::RIGHT_PAREN) {
          delete value;
          goto error;
        }
        return new ParenthesisExpr(value, value->get_source(),value->start_pos(), value->end_pos(),
                                    value->get_line_start());
      }
      case TokenType::DIV_ASSIGN:
      case TokenType::DIV: {  // /
        lexer.cursor_back(); // back to /
        if (token.type == TokenType::DIV_ASSIGN)
          lexer.cursor_back();
        std::u16string pattern, flag;
        token = lexer.scan_regexp_literal(pattern, flag);
        if (token.type == TokenType::REGEX) {
          return new RegExpLiteral(pattern, flag, TOKEN_SOURCE_EXPR);
        }
        else {
          goto error;
        }
        break;
      }
      default:
        goto error;
    }

error:
    return new ASTNode(ASTNode::AST_ILLEGAL, TOKEN_SOURCE_EXPR);
  }

  bool parse_formal_parameter_list(std::vector<std::u16string>& params) {
    // if (!lexer.current_token().is_identifier()) {
    //   // This only happens in new Function(...)
    //   params = {};
    //   return lexer.current_token().type == TokenType::EOS;
    // }
    params.emplace_back(lexer.current().text);
    scope_chain.back().define_func_parameter(lexer.current().text);

    Token token = lexer.next();
    // NOTE(zhuzilin) the EOS is for new Function("a,b,c", "")
    while (token.type != TokenType::RIGHT_PAREN && token.type != TokenType::EOS) {
      if (token.type != TokenType::COMMA) {
        params = {};
        return false;
      }
      token = lexer.next();
      if (!token.is_identifier()) {
        params = {};
        return false;
      }
      params.emplace_back(token.text);
      scope_chain.back().define_func_parameter(token.text);
      token = lexer.next();
    }
    return true;
  }

  ASTNode* parse_function(bool name_required, bool func_keyword_required) {
    START_POS;
    if (func_keyword_required) {
      assert(token_match(u"function"));
      lexer.next();
    }
    else {
      assert(lexer.current().is_identifier());
    }

    Token name = Token::none;
    std::vector<std::u16string> params;
    ASTNode* body;

    if (lexer.current().is_identifier()) {
      name = lexer.current();
      bool res = scope_chain.back().define_symbol(VarKind::DECL_FUNCTION, name.text);
      if (!res) std::cout << "!!!!define symbol " << name.get_text_utf8() << " failed" << std::endl;
      lexer.next();
    }
    else if (name_required) {
      goto error;
    }

    if (!token_match(TokenType::LEFT_PAREN)) goto error;

    push_scope(Scope::FUNC_SCOPE);

    lexer.next();
    if (lexer.current().is_identifier()) {
      if (!parse_formal_parameter_list(params)) {
        goto error;
      }
    }
    if (!token_match(TokenType::RIGHT_PAREN)) goto error;

    if (lexer.next().type != TokenType::LEFT_BRACE) goto error;
    
    lexer.next();
    body = parse_function_body();
    if (body->is_illegal()) return body;

    if (!token_match(TokenType::RIGHT_BRACE)) {
      delete body;
      goto error;
    }
    
    return new Function(name, params, body, SOURCE_PARSED_EXPR);
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_array_literal() {
    START_POS;
    assert(token_match(TokenType::LEFT_BRACK));

    ArrayLiteral* array = new ArrayLiteral();

    // get the token after `[`
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACK) {
      if (token.type != TokenType::COMMA) {
        ASTNode *element = parse_assignment_expression(false);
        if (element->get_type() == ASTNode::AST_ILLEGAL) {
          delete array;
          return element;
        }
        array->add_element(element);
      }
      token = lexer.next();
    }

    assert(token.type == TokenType::RIGHT_BRACK);
    array->set_source(SOURCE_PARSED_EXPR);
    return array;
  }

  ASTNode* parse_object_literal() {
    using ObjectProp = ObjectLiteral::Property;

    START_POS;
    assert(token_match(TokenType::LEFT_BRACE));

    ObjectLiteral* obj = new ObjectLiteral();
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACE) {
      if (token.is_property_name()) {
        // getter or setter
        if ((token.text == u"get" || token.text == u"set") && lexer.peek().is_property_name()) {
          START_POS;

          ObjectProp::Type type = token.text == u"get" ? ObjectLiteral::Property::GET
                                                       : ObjectLiteral::Property::SET;

          Token key = lexer.next();  // skip property name
          if (!key.is_property_name()) goto error;

          ASTNode* get_set_func = parse_function(true, false);
          if (get_set_func->is_illegal()) {
            delete get_set_func;
            delete obj;
            return get_set_func;
          }

          obj->set_property(ObjectProp(key, get_set_func, type));
        }
        else {
          if (lexer.next().type != TokenType::COLON) goto error;
          
          lexer.next();
          ASTNode* value = parse_assignment_expression(false);
          if (value->get_type() == ASTNode::AST_ILLEGAL) {
            delete obj;
            return value;
          }
          obj->set_property(ObjectProp(token, value, ObjectProp::NORMAL));
        }
      }
      else {
        goto error;
      }
      token = lexer.next();
      if (token_match(TokenType::COMMA)) {
        token = lexer.next();  // Skip ,
      }
    }
    assert(token.type == TokenType::RIGHT_BRACE);
    obj->set_source(SOURCE_PARSED_EXPR);
    return obj;
error:
    RENEW_START;
    delete obj;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_expression(bool no_in) {
    START_POS;

    ASTNode* element = parse_assignment_expression(no_in);
    if (element->is_illegal()) {
      return element;
    }
    // NOTE(zhuzilin) If expr has only one element, then just return the element.
    // lexer.checkpoint();
    if (lexer.peek().type != TokenType::COMMA) {
      return element;
    }

    Expression* expr = new Expression();
    expr->add_element(element);
    
    while (lexer.peek().type == TokenType::COMMA) {
      lexer.next();
      lexer.next();
      element = parse_assignment_expression(no_in);
      if (element->is_illegal()) {
        delete expr;
        return element;
      }
      expr->add_element(element);
    }
    expr->set_source(SOURCE_PARSED_EXPR);
    return expr;
  }

  ASTNode* parse_assignment_expression(bool no_in) {
    START_POS;

    ASTNode* lhs = parse_conditional_expression(no_in);
    if (lhs->is_illegal()) return lhs;

    // Not LeftHandSideExpression
    if (lhs->get_type() != ASTNode::AST_EXPR_LHS) {
      return lhs;
    }
    
    Token op = lexer.peek();
    if (!op.is_assignment_operator()) return lhs;
    
    lexer.next();
    lexer.next();
    ASTNode* rhs = parse_assignment_expression(no_in);
    if (rhs->is_illegal()) {
      delete lhs;
      return rhs;
    }

    return new BinaryExpr(lhs, rhs, op, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_conditional_expression(bool no_in) {
    START_POS;
    ASTNode* cond = parse_binary_and_unary_expression(no_in, 0);
    if (cond->is_illegal()) return cond;

    if (lexer.peek().type != TokenType::QUESTION) {
      return cond;
    }
      
    lexer.next();
    lexer.next();
    ASTNode* lhs = parse_assignment_expression(no_in);
    if (lhs->is_illegal()) {
      delete cond;
      return lhs;
    }
    
    if (lexer.next().type != TokenType::COLON) {
      delete cond;
      delete lhs;
      return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    lexer.next();
    ASTNode* rhs = parse_assignment_expression(no_in);
    if (lhs->is_illegal()) {
      delete cond;
      delete lhs;
      return rhs;
    }
    ASTNode* triple = new TernaryExpr(cond, lhs, rhs);
    triple->set_source(SOURCE_PARSED_EXPR);
    return triple;
  }

  ASTNode* parse_binary_and_unary_expression(bool no_in, int priority) {
    START_POS;
    ASTNode* lhs = nullptr;
    ASTNode* rhs = nullptr;
    // Prefix Operators.
    Token prefix_op = lexer.current();
    // NOTE(zhuzilin) !!a = !(!a)
    if (prefix_op.unary_priority() >= priority) {
      lexer.next();
      lhs = parse_binary_and_unary_expression(no_in, prefix_op.unary_priority());
      if (lhs->is_illegal()) return lhs;

      lhs = new UnaryExpr(lhs, prefix_op, true);
    }
    else {
      lhs = parse_left_hand_side_expression(false);
      if (lhs->is_illegal()) return lhs;
      // Postfix Operators.
      //
      // Because the priority of postfix operators are higher than prefix ones,
      // they won't be parsed at the same time.
      
      const Token& postfix_op = lexer.peek();
      if (!lexer.line_term_ahead() && postfix_op.postfix_priority() > priority) {
        lexer.next();
        auto lhs_type = lhs->get_type();

        if (lhs_type != ASTNode::AST_EXPR_BINARY && lhs_type != ASTNode::AST_EXPR_UNARY) {
          lhs = new UnaryExpr(lhs, postfix_op, false);
          lhs->set_source(SOURCE_PARSED_EXPR);
        }
        else {
          delete lhs;
          return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
        }
      }
    }

    Token binary_op = lexer.peek();

    while (binary_op.binary_priority(no_in) > priority) {
      lexer.next();
      lexer.next();
      rhs = parse_binary_and_unary_expression(no_in, binary_op.binary_priority(no_in));
      if (rhs->is_illegal()) {
        delete lhs;
        return rhs;
      }
      lhs = new BinaryExpr(lhs, rhs, binary_op, SOURCE_PARSED_EXPR);
      
      binary_op = lexer.peek();
    }
    
    lhs->set_source(SOURCE_PARSED_EXPR);
    return lhs;
  }

  ASTNode* parse_left_hand_side_expression(bool in_new_expr_ctx = false) {
    START_POS;
    Token token = lexer.current();
    ASTNode* base = nullptr;
    u32 new_count = 0;

    // need test
    if (token.text == u"new") {    
      lexer.next();
      base = parse_left_hand_side_expression(true);
      base = new NewExpr(base, SOURCE_PARSED_EXPR);
    }

    if (base == nullptr && token.text == u"function") {
      base = parse_function(false, true);
    }
    else if (base == nullptr) {
      base = parse_primary_expression();
    }
    if (base->is_illegal()) {
      return base;
    }
    LeftHandSideExpr* lhs = new LeftHandSideExpr(base, new_count);

    while (true) {
      
      token = lexer.peek();
      switch (token.type) {
        case TokenType::LEFT_PAREN: {  // (
          lexer.next();
          ASTNode* ast = parse_arguments();
          if (ast->is_illegal()) {
            delete lhs;
            return ast;
          }
          assert(ast->get_type() == ASTNode::AST_EXPR_ARGS);
          Arguments* args = static_cast<Arguments*>(ast);
          lhs->add_arguments(args);

          if (in_new_expr_ctx) return lhs;
          break;
        }
        case TokenType::LEFT_BRACK: {  // [
          lexer.next();  // skip [
          lexer.next();
          ASTNode* index = parse_expression(false);
          if (index->is_illegal()) {
            delete lhs;
            return index;
          }
          token = lexer.next();  // skip ]
          if (token.type != TokenType::RIGHT_BRACK) {
            delete lhs;
            delete index;
            goto error;
          }
          lhs->add_index(index);
          break;
        }
        case TokenType::DOT: {  // .
          lexer.next();
          token = lexer.next();  // read identifier name
          if (!token.is_identifier_name()) {
            delete lhs;
            goto error;
          }
          lhs->add_prop(token);
          break;
        }
        default:

          lhs->set_source(SOURCE_PARSED_EXPR);
          return lhs;
      }
    }
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_arguments() {
    START_POS;
    assert(token_match(TokenType::LEFT_PAREN));
    std::vector<ASTNode*> args;
    ASTNode* arg;

    lexer.next();
    while (!token_match(TokenType::RIGHT_PAREN)) {
      if (!token_match(TokenType::COMMA)) {
        arg = parse_assignment_expression(false);
        if (arg->is_illegal()) {
          for (auto arg : args) delete arg;
          return arg;
        }
        args.emplace_back(arg);
      }

      lexer.next();
    }

    assert(token_match(TokenType::RIGHT_PAREN));
    Arguments* arg_ast = new Arguments(args);
    arg_ast->set_source(SOURCE_PARSED_EXPR);
    return arg_ast;
error:
    for (auto arg : args) delete arg;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_function_body() {
    return parse_program_or_function_body(TokenType::RIGHT_BRACE, ASTNode::AST_FUNC_BODY);
  }

  ASTNode* parse_program() {
    lexer.next();
    return parse_program_or_function_body(TokenType::EOS, ASTNode::AST_PROGRAM);
  }

  ASTNode* parse_program_or_function_body(TokenType ending_token_type, ASTNode::Type syntax_type) {
    START_POS;
    // 14.1
    bool strict = false;
    
    auto& token_text = lexer.current().text;
    if (token_text == u"\"use strict\"" || token_text == u"'use strict'") {
      if (lexer.try_skip_semicolon()) {
        lexer.next();
        strict = true;
        // and `curr_token` will be the token following 'use strict'.
      }
      // else, `curr_token` will be 'use strict' (a string).
    }

    if (syntax_type == ASTNode::AST_PROGRAM) push_scope(Scope::GLOBAL_SCOPE);

    ProgramOrFunctionBody* prog = new ProgramOrFunctionBody(syntax_type, strict);
    ASTNode* statement;
    
    while (!token_match(ending_token_type)) {
      statement = parse_statement();
      
      if (statement->is_illegal()) {
        delete prog;
        return statement;
      }

      if (statement->type == ASTNode::AST_FUNC) {
        prog->add_function_decl(statement);
      }
      else {
        prog->add_statement(statement);
      }

      lexer.next();
    }
    assert(token_match(ending_token_type));
    
    #ifndef DBG_SCOPE
    prog->scope = std::move(scope_chain.back());
    #endif
    pop_scope();

    prog->set_source(SOURCE_PARSED_EXPR);
    return prog;
  }

  ASTNode* parse_statement() {
    START_POS;
    const Token& token = lexer.current();

    switch (token.type) {
      case TokenType::LEFT_BRACE:  // {
        return parse_block_statement();
      case TokenType::SEMICOLON:  // ;
        return new ASTNode(ASTNode::AST_STMT_EMPTY, TOKEN_SOURCE_EXPR);
      case TokenType::KEYWORD: {
        if (token.text == u"var" || token.text == u"let" || token.text == u"const") {
          return parse_variable_statement(false);
        }
        else if (token.text == u"if") return parse_if_statement();
        else if (token.text == u"do") return parse_do_while_statement();
        else if (token.text == u"while") return parse_while_statement();
        else if (token.text == u"for") return parse_for_statement();
        else if (token.text == u"continue") return parse_continue_statement();
        else if (token.text == u"break") return parse_break_statement();
        else if (token.text == u"return") return parse_return_statement();
        else if (token.text == u"with") return parse_with_statement();
        else if (token.text == u"switch") return parse_switch_statement();
        else if (token.text == u"throw") return parse_throw_statement();
        else if (token.text == u"try") return parse_try_statement();
        else if (token.text == u"function") {
          auto func = parse_function(true, true);
          if (func->type == ASTNode::AST_FUNC) {
            static_cast<Function*>(func)->is_stmt = true;
          }
          return func;
        }
        else if (token.text == u"debugger") {
          if (!lexer.try_skip_semicolon()) {
            goto error;
          }
          return new ASTNode(ASTNode::AST_STMT_DEBUG, SOURCE_PARSED_EXPR);
        }
        break;
      }
      case TokenType::STRICT_FUTURE_KW:
      case TokenType::IDENTIFIER: {
        
        Token colon = lexer.peek();
        if (colon.type == TokenType::COLON) {
          return parse_labelled_statement();
        }
      }
      default:
        break;
    }
    return parse_expression_statement();
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_block_statement() {
    START_POS;
    assert(token_match(TokenType::LEFT_BRACE));
    Block* block = new Block();
    lexer.next();

    push_scope(Scope::BLOCK_SCOPE);

    while (!token_match(TokenType::RIGHT_BRACE)) {
      ASTNode* stmt = parse_statement();
      if (stmt->is_illegal()) {
        delete block;
        return stmt;
      }
      block->add_statement(stmt);
      lexer.next();
    }
    assert(token_match(TokenType::RIGHT_BRACE));
    block->set_source(SOURCE_PARSED_EXPR);

    #ifndef DBG_SCOPE
    block->scope = std::move(scope_chain.back());
    #endif
    pop_scope();

    return block;
  }

  ASTNode* parse_variable_declaration(bool no_in, VarKind kind) {
    START_POS;
    Token id = lexer.current();
    
    assert(id.is_identifier());

    if (lexer.peek().type != TokenType::ASSIGN) {
      if (kind == VarKind::DECL_CONST) {
        lexer.next();
        return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
      VarDecl* var_decl = new VarDecl(id, SOURCE_PARSED_EXPR);
      
      bool res = scope_chain.back().define_symbol(kind, id.text);
      if (!res) std::cout << "!!!!define symbol " << id.get_text_utf8() << " failed" << std::endl;
      return var_decl;
    }
    
    ASTNode* init = parse_assignment_expression(no_in);
    if (init->is_illegal()) return init;
    
    VarDecl* var_decl = new VarDecl(id, init, SOURCE_PARSED_EXPR);
    
    bool res = scope_chain.back().define_symbol(kind, id.text);
    if (!res) std::cout << "!!!!define symbol " << id.get_text_utf8() << " failed" << std::endl;
    return var_decl;
  }

  ASTNode* parse_variable_statement(bool no_in) {
    START_POS;
    auto var_kind_text = lexer.current().text;
    VarKind var_kind = get_var_kind_from_str(var_kind_text);
    
    VarStatement* var_stmt = new VarStatement(var_kind);
    ASTNode* decl;
    if (!lexer.next().is_identifier()) {
      goto error;
    }

    while (true) {

      if (lexer.current().text != u",") {
        decl = parse_variable_declaration(no_in, var_kind);
        if (decl->is_illegal()) {
          delete var_stmt;
          return decl;
        }
        var_stmt->add_decl(decl);
      }
      if (lexer.try_skip_semicolon()) break;
      lexer.next();
    }

    var_stmt->set_source(SOURCE_PARSED_EXPR);
    return var_stmt;
error:
    delete var_stmt;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_expression_statement() {
    START_POS;
    const Token& token = lexer.current();
    if (token.text == u"function") {
      return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    // already handled LEFT_BRACE case in caller.
    assert(token.type != TokenType::LEFT_BRACE);
    ASTNode* exp = parse_expression(false);
    if (exp->is_illegal()) return exp;

    if (!lexer.try_skip_semicolon()) {
      delete exp;
      return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    return exp;
  }

  ASTNode* parse_if_statement() {
    START_POS;
    ASTNode* cond;
    ASTNode* if_block;

    assert(token_match(u"if"));
    lexer.next();
    lexer.next();
    cond = parse_expression(false);
    if (cond->is_illegal()) return cond;

    if (lexer.next().type != TokenType::RIGHT_PAREN) {
      delete cond;
      goto error;
    }
    lexer.next();
    if_block = parse_statement();
    if (if_block->is_illegal()) {
      delete cond;
      return if_block;
    }
    
    if (lexer.peek().text == u"else") {
      lexer.next();
      lexer.next();
      ASTNode* else_block = parse_statement();
      if (else_block->is_illegal()) {
        delete cond;
        delete if_block;
        return else_block;
      }
      return new IfStatement(cond, if_block, else_block, SOURCE_PARSED_EXPR);
    }

    return new IfStatement(cond, if_block, SOURCE_PARSED_EXPR);
    
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_do_while_statement() {
    START_POS;
    assert(token_match(u"do"));
    ASTNode* cond;
    ASTNode* loop_block;
    lexer.next();
    loop_block = parse_statement();
    if (loop_block->is_illegal()) {
      return loop_block;
    }
    if (lexer.next().text != u"while") {  // skip while
      delete loop_block;
      goto error;
    }
    if (lexer.next().type != TokenType::LEFT_PAREN) {  // skip (
      delete loop_block;
      goto error;
    }
    lexer.next();
    cond = parse_expression(false);
    if (cond->is_illegal()) {
      delete loop_block;
      return cond;
    }
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      delete cond;
      goto error;
    }
    if (!lexer.try_skip_semicolon()) {
      delete cond;
      delete loop_block;
      goto error;
    }
    return new DoWhileStatement(cond, loop_block, SOURCE_PARSED_EXPR);
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_while_statement() {
    return parse_while_or_with_statement(ASTNode::AST_STMT_WHILE);
  }

  ASTNode* parse_with_statement() {
    return parse_while_or_with_statement(ASTNode::AST_STMT_WITH);
  }

  ASTNode* parse_while_or_with_statement(ASTNode::Type type) {
    START_POS;
    std::u16string keyword = type == ASTNode::AST_STMT_WHILE ? u"while" : u"with";
    assert(token_match(keyword));
    ASTNode* expr;
    ASTNode* stmt;
    if (lexer.next().type != TokenType::LEFT_PAREN) { // read (
      goto error;
    }

    lexer.next();
    expr = parse_expression(false);
    if (expr->is_illegal()) return expr;

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // read )
      delete expr;
      goto error;
    }
    lexer.next();
    stmt = parse_statement();
    if (stmt->is_illegal()) {
      delete expr;
      return stmt;
    }
    if (type == ASTNode::AST_STMT_WHILE) {
      return new WhileStatement(expr, stmt, SOURCE_PARSED_EXPR);
    }
    return new WithStatement(expr, stmt, SOURCE_PARSED_EXPR);
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_for_statement() {
    START_POS;
    assert(token_match(u"for"));

    ASTNode* init_expr;
    if (lexer.next().type != TokenType::LEFT_PAREN) goto error;

    lexer.next();
    if (lexer.current().is_semicolon()) {
      return parse_for_statement({}, start, line_start);  // for (;
    }
    else if (token_match(u"var") || token_match(u"let") || token_match(u"const")) {
      VarKind var_kind = get_var_kind_from_str(lexer.current().text);
      lexer.next();  // skip var
      std::vector<ASTNode*> init_expressions;

      // NOTE(zhuzilin) the starting token for parse_variable_declaration
      // must be identifier. This is for better error code.
      if (!lexer.current().is_identifier()) goto error;

      init_expr = parse_variable_declaration(true, var_kind);
      if (init_expr->is_illegal()) return init_expr;

      // expect `in`, `,`
      // the `in` case
      // var VariableDeclarationNoIn in
      if (lexer.next().text == u"in") return parse_for_in_statement(init_expr, start, line_start);

      // the `,` case
      init_expressions.emplace_back(init_expr);
      while (!lexer.current().is_semicolon()) {
        // NOTE(zhuzilin) the starting token for parse_variable_declaration
        // must be identifier. This is for better error code.
        if (!token_match(TokenType::COMMA) || !lexer.next().is_identifier()) {
          for (auto expr : init_expressions) {
            delete expr;
          }
          goto error;
        }

        init_expr = parse_variable_declaration(true, var_kind);
        if (init_expr->is_illegal()) {
          for (auto expr : init_expressions)
            delete expr;
          return init_expr;
        }
        init_expressions.emplace_back(init_expr);
        lexer.next();
      } 
      // for (var VariableDeclarationListNoIn; ...)
      return parse_for_statement(init_expressions, start, line_start);
    }
    else {
      init_expr = parse_expression(true);
      if (init_expr->is_illegal()) {
        return init_expr;
      }
      lexer.next();
      if (lexer.current().is_semicolon()) {
        return parse_for_statement({init_expr}, start, line_start);  // for ( ExpressionNoIn;
      }
      // for ( LeftHandSideExpression in
      else if (token_match(u"in") && init_expr->get_type() == ASTNode::AST_EXPR_LHS) {
        return parse_for_in_statement(init_expr, start, line_start);  
      }
      else {
        delete init_expr;
        goto error;
      }
    }
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_for_statement(std::vector<ASTNode*> init_expressions, u32 start, u32 line_start) {
    assert(lexer.current().is_semicolon());
    ASTNode* expr1 = nullptr;
    ASTNode* expr2 = nullptr;
    ASTNode* stmt = nullptr;

    if (!lexer.peek().is_semicolon()) {
      lexer.next();
      expr1 = parse_expression(false);  // for (xxx; Expression
      if (expr1->is_illegal()) {
        for (auto expr : init_expressions) {
          delete expr;
        }
        return expr1;
      }
    }

    if (!lexer.next().is_semicolon()) {  // read the second ;
      goto error;
    }

    if (lexer.peek().type != TokenType::RIGHT_PAREN) {
      lexer.next();
      expr2 = parse_expression(false);  // for (xxx; xxx; Expression
      if (expr2->is_illegal()) {
        for (auto expr : init_expressions) {
          delete expr;
        }
        if (expr1 != nullptr)
          delete expr1;
        return expr2;
      }
    }

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // read the )
      goto error;
    }

    lexer.next();
    stmt = parse_statement();
    if (stmt->is_illegal()) {
      for (auto expr : init_expressions) {
        delete expr;
      }
      if (expr1 != nullptr) delete expr1;
      if (expr2 != nullptr) delete expr2;
      return stmt;
    }

    return new ForStatement(init_expressions, expr1, expr2, stmt, SOURCE_PARSED_EXPR);
error:
    for (auto expr : init_expressions) {
      delete expr;
    }
    if (expr1 != nullptr) delete expr1;
    if (expr2 != nullptr) delete expr2;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_for_in_statement(ASTNode* expr0, u32 start, u32 line_start) {
    assert(token_match(u"in"));
    lexer.next();
    ASTNode* expr1 = parse_expression(false);  // for ( xxx in Expression
    ASTNode* stmt;
    if (expr1->is_illegal()) {
      delete expr0;
      return expr1;
    }

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      goto error;
    }

    lexer.next();
    stmt = parse_statement();
    if (stmt->is_illegal()) {
      delete expr0;
      delete expr1;
      return stmt;
    }
    return new ForInStatement(expr0, expr1, stmt, SOURCE_PARSED_EXPR);
error:
    delete expr0;
    delete expr1;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_continue_statement() {
    return parse_continue_or_break_statement(ASTNode::AST_STMT_CONTINUE);
  }

  ASTNode* parse_break_statement() {
    return parse_continue_or_break_statement(ASTNode::AST_STMT_BREAK);
  }

  ASTNode* parse_continue_or_break_statement(ASTNode::Type type) {
    START_POS;
    std::u16string_view keyword = type == ASTNode::AST_STMT_CONTINUE ? u"continue" : u"break";
    assert(token_match(keyword));
    if (!lexer.try_skip_semicolon()) {
      Token id = lexer.next();
      if (!id.is_identifier()) {
        return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
      if (!lexer.try_skip_semicolon()) {
        return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
      return new ContinueOrBreak(type, id, SOURCE_PARSED_EXPR);
    }
    return new ContinueOrBreak(type, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_return_statement() {
    START_POS;
    assert(token_match(u"return"));
    ASTNode* expr = nullptr;
    
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      expr = parse_expression(false);
      if (expr->is_illegal()) {
        return expr;
      }
      if (!lexer.try_skip_semicolon()) {
        delete expr;
        return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
    }
    return new ReturnStatement(expr, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_throw_statement() {
    START_POS;
    assert(token_match(u"throw"));
    ASTNode* expr = nullptr;
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      expr = parse_expression(false);
      if (expr->is_illegal()) {
        return expr;
      }
      if (!lexer.try_skip_semicolon()) {
        delete expr;
        return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
    }
    return new ThrowStatement(expr, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_switch_statement() {
    START_POS;
    SwitchStatement* switch_stmt = new SwitchStatement();
    ASTNode* expr = nullptr;
    // Token token = lexer.current();
    assert(token_match(u"switch"));
    if (lexer.next().type != TokenType::LEFT_PAREN) { // skip (
      goto error;
    }
    lexer.next();
    expr = parse_expression(false);
    if (expr->is_illegal()) {
      delete switch_stmt;
      return expr;
    }
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      delete expr;
      goto error;
    }
    switch_stmt->set_expr(expr);
    if (lexer.next().type != TokenType::LEFT_BRACE) { // skip {
      goto error;
    }
    // Loop for parsing CaseClause
    lexer.next();
    while (!token_match(TokenType::RIGHT_BRACE)) {
      ASTNode* case_expr = nullptr;
      std::vector<ASTNode*> stmts;
      std::u16string_view type = lexer.current().text;
      if (type == u"case") {
        lexer.next();  // skip case
        case_expr = parse_expression(false);
        if (case_expr->is_illegal()) {
          delete switch_stmt;
          return case_expr;
        }
      }
      else if (type == u"default") {
        // can only have one default.
        if (switch_stmt->has_default_clause) goto error;
      }
      else {
        goto error;
      }
      if (lexer.next().type != TokenType::COLON) { // skip :
        delete case_expr;
        goto error;
      }
      // parse StatementList
      lexer.next();
      // the statements in each case
      while (!token_match(u"case") && !token_match(u"default") &&
              !token_match(TokenType::RIGHT_BRACE)) {
        ASTNode* stmt = parse_statement();
        if (stmt->is_illegal()) {
          for (auto s : stmts) {
            delete s;
          }
          delete switch_stmt;
          return stmt;
        }
        stmts.emplace_back(stmt);
        lexer.next();
      }
      if (type == u"case") {
        if (switch_stmt->has_default_clause) {
          switch_stmt->add_after_default_clause(SwitchStatement::CaseClause(case_expr, stmts));
        }
        else {
          switch_stmt->add_before_default_clause(SwitchStatement::CaseClause(case_expr, stmts));
        }
      }
      else {
        switch_stmt->set_default_clause(stmts);
      }
    }

    assert(token_match(TokenType::RIGHT_BRACE));
    switch_stmt->set_source(SOURCE_PARSED_EXPR);
    return switch_stmt;
error:
    delete switch_stmt;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_try_statement() {
    START_POS;
    assert(token_match(u"try"));

    ASTNode* try_block = nullptr;
    Token catch_id = Token::none;
    ASTNode* catch_block = nullptr;
    ASTNode* finally_block = nullptr;

    if (lexer.next().type != TokenType::LEFT_BRACE) {
      goto error;
    }
    try_block = parse_block_statement();
    if (try_block->is_illegal())
      return try_block;
    
    if (lexer.peek().text == u"catch") {
      lexer.next();
      if (lexer.next().type != TokenType::LEFT_PAREN) {  // skip (
        delete try_block;
        goto error;
      }
      catch_id = lexer.next();  // skip identifier
      if (!catch_id.is_identifier()) {
        goto error;
      }
      if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
        delete try_block;
        goto error;
      }
      lexer.next();
      catch_block = parse_block_statement();
      if (catch_block->is_illegal()) {
        delete try_block;
        return catch_block;
      }
    }
    if (lexer.peek().text == u"finally") {
      lexer.next();
      if (lexer.next().type != TokenType::LEFT_BRACE) {
        goto error;
      }
      finally_block = parse_block_statement();
      if (finally_block->is_illegal()) {
        delete try_block;

        if (catch_block != nullptr) delete catch_block;
        return finally_block;
      }
    }

    if (catch_block == nullptr && finally_block == nullptr) {
      goto error;
    }
    else if (finally_block == nullptr) {
      assert(catch_block != nullptr && catch_id.is_identifier());
      return new TryStatement(try_block, catch_id, catch_block, SOURCE_PARSED_EXPR);
    }
    else if (catch_block == nullptr) {
      assert(finally_block != nullptr);
      return new TryStatement(try_block, finally_block, SOURCE_PARSED_EXPR);
    }
    assert(catch_block != nullptr && catch_id.is_identifier());
    assert(finally_block != nullptr);
    return new TryStatement(try_block, catch_id, catch_block, finally_block, SOURCE_PARSED_EXPR);
error:
    if (try_block != nullptr)
      delete try_block;
    if (catch_block != nullptr)
      delete try_block;
    if (finally_block != nullptr)
      delete finally_block;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_labelled_statement() {
    START_POS;
    Token id = lexer.current();  // skip identifier
    lexer.next();
    assert(token_match(TokenType::COLON));  // skip colon
    lexer.next();
    ASTNode* stmt = parse_statement();

    if (stmt->is_illegal()) return stmt;
    return new LabelledStatement(id, stmt, SOURCE_PARSED_EXPR);
  }

 private:

  inline bool token_match(u16string_view token) {
    return lexer.current().text == token;
  }

  inline bool token_match(TokenType type) {
    return lexer.current().type == type;
  }

  std::u16string source;
  Lexer lexer;

  ParserContext context;

  SmallVector<Scope, 10> scope_chain;

  Scope& current_scope() { return scope_chain.back(); }

  void push_scope(Scope::Type scope_type) {
    Scope *parent = scope_chain.size() > 0 ? &scope_chain.back() : nullptr;
    scope_chain.emplace_back(scope_type, parent);

    #ifdef DBG_SCOPE
    std::cout << "push scope: " << scope_chain.back().get_scope_type_name() << std::endl;
    std::cout << std::endl;
    #endif
  }

  void pop_scope() {
    #ifdef DBG_SCOPE
    Scope& scope = scope_chain.back();

    std::cout << "pop scope: " << scope.get_scope_type_name() << std::endl;
    std::cout << "params count: " << scope.param_count
              << ", local variables count: " << scope.local_var_count << std::endl;
    std::cout << "symbols in this scope: " << std::endl;

    for (auto& entry : scope.get_symbol_table()) {
      std::cout << get_var_kind_str(entry.second.var_kind) << "  "
                << to_utf8_string(entry.second.name) << "  "
                << entry.second.index << "  "
                << std::endl;
    }
    std::cout << std::endl;

    #endif

    scope_chain.pop_back();
  }
};

}  // namespace njs

#endif  // NJS_PARSER_H