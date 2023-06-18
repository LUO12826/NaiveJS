#ifndef NJS_PARSER_H
#define NJS_PARSER_H

#include <stack>

#include "njs/parser/lexer.h"
#include "njs/parser/ast.h"
#include "njs/utils/helper.h"

#define START_POS u32 start = lexer.current_pos()
#define SOURCE_PARSED_EXPR source.substr(start, lexer.current_pos() - start), start, lexer.current_pos()
#define TOKEN_SOURCE_EXPR token.text, token.start, token.end

namespace njs {

class Parser {
 public:
  Parser(std::u16string source) : source(source), lexer(source) {
    // For test
    var_decl_stack.push({});
  }

  AST* ParsePrimaryExpression() {
    Token token = lexer.peek();
    switch (token.type) {
      case TokenType::KEYWORD:
        if (token.text == u"this") {
          lexer.next();
          return new AST(AST::AST_EXPR_THIS, TOKEN_SOURCE_EXPR);
        }
        goto error;
      case TokenType::STRICT_FUTURE_KW:
        lexer.next();
        return new AST(AST::AST_EXPR_STRICT_FUTURE, TOKEN_SOURCE_EXPR);
      case TokenType::IDENTIFIER:
        lexer.next();
        return new AST(AST::AST_EXPR_IDENT, TOKEN_SOURCE_EXPR);
      case TokenType::TK_NULL:
        lexer.next();
        return new AST(AST::AST_EXPR_NULL, TOKEN_SOURCE_EXPR);
      case TokenType::TK_BOOL:
        lexer.next();
        return new AST(AST::AST_EXPR_BOOL, TOKEN_SOURCE_EXPR);
      case TokenType::NUMBER:
        lexer.next();
        return new AST(AST::AST_EXPR_NUMBER, TOKEN_SOURCE_EXPR);
      case TokenType::STRING:
        lexer.next();
        return new AST(AST::AST_EXPR_STRING, TOKEN_SOURCE_EXPR);
      case TokenType::LEFT_BRACK:  // [
        return ParseArrayLiteral();
      case TokenType::LEFT_BRACE:  // {
        return ParseObjectLiteral();
      case TokenType::LEFT_PAREN: { // (
        lexer.next();   // skip (
        AST* value = ParseExpression(false);
        if (value->IsIllegal())
          return value;
        if (lexer.next().type != TokenType::RIGHT_PAREN) {
          delete value;
          goto error;
        }
        return new Paren(value, value->source(), value->start(), value->end());
      }
      case TokenType::DIV_ASSIGN:
      case TokenType::DIV: {  // /
        lexer.next(); // skip /
        lexer.cursor_back(); // back to /
        if (token.type == TokenType::DIV_ASSIGN)
          lexer.cursor_back();
        std::u16string pattern, flag;
        token = lexer.scan_regexp_literal(pattern, flag);
        if (token.type == TokenType::REGEX) {
          return new RegExpLiteral(pattern, flag, TOKEN_SOURCE_EXPR);
        } else {
          goto error;
        }
        break;
      }
      default:
        goto error;
    }

error:
    return new AST(AST::AST_ILLEGAL, TOKEN_SOURCE_EXPR);
  }

  // done
  bool ParseFormalParameterList(std::vector<std::u16string>& params) {
    // if (!lexer.current_token().is_identifier()) {
    //   // This only happens in new Function(...)
    //   params = {};
    //   return lexer.current_token().type == TokenType::EOS;
    // }
    params.emplace_back(lexer.current().text);
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
      token = lexer.next();
    }
    return true;
  }

  AST* ParseFunction(bool must_be_named) {
    START_POS;
    assert(lexer.current().text == u"function");

    Token name = Token::none;
    std::vector<std::u16string> params;
    AST* body;

    // Identifier_opt
    // Token token = lexer.next();
    if (lexer.next().is_identifier()) {
      name = lexer.current();
      // expect LEFT_PAREN
      // token = lexer.next();
    }
    else if (must_be_named) {
      goto error;
    }
    if (lexer.next().type != TokenType::LEFT_PAREN) {
      goto error;
    }
    // token = lexer.next();
    if (lexer.next().is_identifier()) {
      if (!ParseFormalParameterList(params)) {
        goto error;
      }
    }
    if (lexer.current().type != TokenType::RIGHT_PAREN) {
      goto error;
    }
    // token = lexer.next();
    if (lexer.next().type != TokenType::LEFT_BRACE) {
      goto error;
    }
    body = ParseFunctionBody();
    if (body->IsIllegal())
      return body;

    // token = lexer.next();
    if (lexer.next().type != TokenType::RIGHT_BRACE) {
      goto error;
    }

    return new Function(name, params, body, SOURCE_PARSED_EXPR);
error:
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseArrayLiteral() {
    START_POS;
    assert(lexer.next().type == TokenType::LEFT_BRACK);

    ArrayLiteral* array = new ArrayLiteral();
    AST* element = nullptr;

    Token token = lexer.peek();
    while (token.type != TokenType::RIGHT_BRACK) {
      switch (token.type) {
        case TokenType::COMMA:
          lexer.next();
          array->AddElement(element);
          element = nullptr;
          break;
        default:
          element = ParseAssignmentExpression(false);
          if (element->type() == AST::AST_ILLEGAL) {
            delete array;
            return element;
          }
      }
      token = lexer.peek();
    }
    if (element != nullptr) {
      array->AddElement(element);
    }
    assert(token.type == TokenType::RIGHT_BRACK);
    assert(lexer.next().type == TokenType::RIGHT_BRACK);
    array->SetSource(SOURCE_PARSED_EXPR);
    return array;
  }

  AST* ParseObjectLiteral() {
    START_POS;
    assert(lexer.next().type == TokenType::LEFT_BRACE);

    ObjectLiteral* obj = new ObjectLiteral();
    Token token = lexer.peek();
    while (token.type != TokenType::RIGHT_BRACE) {
      if (token.is_property_name()) {
        lexer.next();
        if ((token.text == u"get" || token.text == u"set") &&
            lexer.peek().is_property_name()) {
          START_POS;
          ObjectLiteral::Property::Type type;
          if (token.text == u"get")
            type = ObjectLiteral::Property::GET;
          else
            type = ObjectLiteral::Property::SET;
          Token key = lexer.next();  // skip property name
          if (!key.is_property_name()) {
            goto error;
          }
          if (lexer.next().type != TokenType::LEFT_PAREN) {
            goto error;
          }
          std::vector<std::u16string> params;
          if (type == ObjectLiteral::Property::SET) {
            Token param = lexer.next();
            if (!param.is_identifier()) {
              goto error;
            }
            params.emplace_back(param.text);
          }
          if (lexer.next().type != TokenType::RIGHT_PAREN) { // Skip )
            goto error;
          }
          if (lexer.next().type != TokenType::LEFT_BRACE) { // Skip {
            goto error;
          }
          AST* body = ParseFunctionBody();
          if (body->IsIllegal()) {
            delete obj;
            return body;
          }
          if (lexer.next().type != TokenType::RIGHT_BRACE) { // Skip }
            delete body;
            goto error;
          }
          Function* value = new Function(Token::none, params, body, SOURCE_PARSED_EXPR);
          obj->AddProperty(ObjectLiteral::Property(key, value, type));
        } else {
          if (lexer.next().type != TokenType::COLON)
            goto error;
          AST* value = ParseAssignmentExpression(false);
          if (value->type() == AST::AST_ILLEGAL)
            goto error;
          obj->AddProperty(ObjectLiteral::Property(token, value, ObjectLiteral::Property::NORMAL));
        }
      } else {
        lexer.next();
        goto error;
      }
      token = lexer.peek();
      if (token.type == TokenType::COMMA) {
        lexer.next();  // Skip ,
        token = lexer.peek();
      }
    }
    assert(token.type == TokenType::RIGHT_BRACE);
    assert(lexer.next().type == TokenType::RIGHT_BRACE);
    obj->SetSource(SOURCE_PARSED_EXPR);
    return obj;
error:
    delete obj;
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseExpression(bool no_in) {
    START_POS;

    AST* element = ParseAssignmentExpression(no_in);
    if (element->IsIllegal()) {
      return element;
    }
    // NOTE(zhuzilin) If expr has only one element, then just return the element.
    Token token = lexer.peek();
    if (token.type != TokenType::COMMA) {
      return element;
    }

    Expression* expr = new Expression();
    expr->AddElement(element);
    while (token.type == TokenType::COMMA) {
      lexer.next();  // skip ,
      element = ParseAssignmentExpression(no_in);
      if (element->IsIllegal()) {
        delete expr;
        return element;
      }
      expr->AddElement(element);
      token = lexer.peek();
    }
    expr->SetSource(SOURCE_PARSED_EXPR);
    return expr;
  }

  AST* ParseAssignmentExpression(bool no_in) {
    START_POS;

    AST* lhs = ParseConditionalExpression(no_in);
    if (lhs->IsIllegal())
      return lhs;

    // Not LeftHandSideExpression
    if (lhs->type() != AST::AST_EXPR_LHS) {
      return lhs;
    }
    Token op = lexer.peek();
    if (!op.is_assignment_operator())
      return lhs;

    lexer.next();
    AST* rhs = ParseAssignmentExpression(no_in);
    if (rhs->IsIllegal()) {
      delete lhs;
      return rhs;
    }

    return new Binary(lhs, rhs, op, SOURCE_PARSED_EXPR);
  }

  AST* ParseConditionalExpression(bool no_in) {
    START_POS;
    AST* cond = ParseBinaryAndUnaryExpression(no_in, 0);
    if (cond->IsIllegal())
      return cond;
    Token token = lexer.peek();
    if (token.type != TokenType::QUESTION)
      return cond;
    lexer.next();
    AST* lhs = ParseAssignmentExpression(no_in);
    if (lhs->IsIllegal()) {
      delete cond;
      return lhs;
    }
    token = lexer.peek();
    if (token.type != TokenType::COLON) {
      delete cond;
      delete lhs;
      return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    lexer.next();
    AST* rhs = ParseAssignmentExpression(no_in);
    if (lhs->IsIllegal()) {
      delete cond;
      delete lhs;
      return rhs;
    }
    AST* triple = new TripleCondition(cond, lhs, rhs);
    triple->SetSource(SOURCE_PARSED_EXPR);
    return triple;
  }

  AST* ParseBinaryAndUnaryExpression(bool no_in, int priority) {
    START_POS;
    AST* lhs = nullptr;
    AST* rhs = nullptr;
    // Prefix Operators.
    Token prefix_op = lexer.peek();
    // NOTE(zhuzilin) !!a = !(!a)
    if (prefix_op.unary_priority() >= priority) {
      lexer.next();
      lhs = ParseBinaryAndUnaryExpression(no_in, prefix_op.unary_priority());
      if (lhs->IsIllegal())
        return lhs;
      lhs = new Unary(lhs, prefix_op, true);
    }
    else {
      lhs = ParseLeftHandSideExpression();
      if (lhs->IsIllegal()) return lhs;
      // Postfix Operators.
      //
      // Because the priority of postfix operators are higher than prefix ones,
      // they won't be parsed at the same time.
      Token postfix_op = lexer.peek();
      if (!lexer.line_term_ahead() && postfix_op.postfix_priority() > priority) {
        if (lhs->type() != AST::AST_EXPR_BINARY && lhs->type() != AST::AST_EXPR_UNARY) {
          lexer.next();
          lhs = new Unary(lhs, postfix_op, false);
          lhs->SetSource(SOURCE_PARSED_EXPR);
        } else {
          delete lhs;
          return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
        }
      }
    }
    while (true) {
      Token binary_op = lexer.peek();
      if (binary_op.binary_priority(no_in) > priority) {
        lexer.next();
        rhs = ParseBinaryAndUnaryExpression(no_in, binary_op.binary_priority(no_in));
        if (rhs->IsIllegal())
          return rhs;
        lhs = new Binary(lhs, rhs, binary_op, SOURCE_PARSED_EXPR);
      } else {
        break;
      }
    }
    lhs->SetSource(SOURCE_PARSED_EXPR);
    return lhs;
  }

  AST* ParseLeftHandSideExpression() {
    START_POS;
    Token token = lexer.current();
    AST* base;
    u32 new_count = 0;

    // need test
    while (token.text == u"new") {
      new_count++;
      token = lexer.next();
    }

    if (token.text == u"function") {
      base = ParseFunction(false);
    }
    else {
      base = ParsePrimaryExpression();
    }
    if (base->IsIllegal()) {
      return base;
    }
    LHS* lhs = new LHS(base, new_count);

    while (true) {
      token = lexer.peek();
      switch (token.type) {
        case TokenType::LEFT_PAREN: {  // (
          AST* ast = ParseArguments();
          if (ast->IsIllegal()) {
            delete lhs;
            return ast;
          }
          assert(ast->type() == AST::AST_EXPR_ARGS);
          Arguments* args = static_cast<Arguments*>(ast);
          lhs->AddArguments(args);
          break;
        }
        case TokenType::LEFT_BRACK: {  // [
          lexer.next();  // skip [
          AST* index = ParseExpression(false);
          if (index->IsIllegal()) {
            delete lhs;
            return index;
          }
          token = lexer.next();  // skip ]
          if (token.type != TokenType::RIGHT_BRACK) {
            delete lhs;
            delete index;
            goto error;
          }
          lhs->AddIndex(index);
          break;
        }
        case TokenType::DOT: {  // .
          lexer.next();  // skip .
          token = lexer.next();  // skip IdentifierName
          if (!token.is_identifier_name()) {
            delete lhs;
            goto error;
          }
          lhs->AddProp(token);
          break;
        }
        default:
          lhs->SetSource(SOURCE_PARSED_EXPR);
          return lhs;
      }
    }
error:
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseArguments() {
    START_POS;
    assert(lexer.next().type == TokenType::LEFT_PAREN);
    std::vector<AST*> args;
    AST* arg;
    Arguments* arg_ast;
    Token token = lexer.peek();
    if (token.type != TokenType::RIGHT_PAREN) {
      arg = ParseAssignmentExpression(false);
      if (arg->IsIllegal())
        return arg;
      args.emplace_back(arg);
      token = lexer.peek();
    }
    while (token.type != TokenType::RIGHT_PAREN) {
      if (token.type != TokenType::COMMA) {
        goto error;
      }
      lexer.next();  // skip ,
      arg = ParseAssignmentExpression(false);
      if (arg->IsIllegal()) {
        for (auto arg : args)
          delete arg;
        return arg;
      }
      args.emplace_back(arg);
      token = lexer.peek();
    }
    assert(lexer.next().type == TokenType::RIGHT_PAREN);  // skip )
    arg_ast = new Arguments(args);
    arg_ast->SetSource(SOURCE_PARSED_EXPR);
    return arg_ast;
error:
    for (auto arg : args)
      delete arg;
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseFunctionBody(TokenType ending_token_type = TokenType::RIGHT_BRACE) {
    return ParseProgramOrFunctionBody(ending_token_type, AST::AST_FUNC_BODY);
  }

  AST* ParseProgram() {
    lexer.next();
    return ParseProgramOrFunctionBody(TokenType::EOS, AST::AST_PROGRAM);
  }

  AST* ParseProgramOrFunctionBody(TokenType ending_token_type, AST::Type program_or_function) {
    START_POS;
    // 14.1
    bool strict = false;
    
    Token token = lexer.current();
    if (token.text == u"\"use strict\"" || token.text == u"'use strict'") {
      if (lexer.try_skip_semicolon()) {
        strict = true;
        // and `curr_token` will be the token following 'use strict'.
      }
      // else, `curr_token` will be 'use strict' (a string).
    }

    ProgramOrFunctionBody* prog = new ProgramOrFunctionBody(program_or_function, strict);
    var_decl_stack.push({});

    AST* element;
    
    while (token.type != ending_token_type) {
      if (token.text == u"function") {
        element = ParseFunction(true);
        if (element->IsIllegal()) {
          delete prog;
          return element;
        }
        prog->AddFunctionDecl(element);
      }
      else {
        element = ParseStatement();
        if (element->IsIllegal()) {
          delete prog;
          return element;
        }
        prog->AddStatement(element);
      }
      token = lexer.next();
    }
    assert(token.type == ending_token_type);
    prog->SetVarDecls(std::move(var_decl_stack.top()));
    var_decl_stack.pop();
    prog->SetSource(SOURCE_PARSED_EXPR);
    return prog;
  }

  AST* ParseStatement() {
    START_POS;
    Token& const token = lexer.current();

    switch (token.type) {
      case TokenType::LEFT_BRACE:  // {
        return ParseBlockStatement();
      case TokenType::SEMICOLON:  // ;
        lexer.next();
        return new AST(AST::AST_STMT_EMPTY, TOKEN_SOURCE_EXPR);
      case TokenType::KEYWORD: {
        if (token.text == u"var") {
          lexer.next();
          return ParseVariableStatement(false);
        }
        else if (token.text == u"if") {
          return ParseIfStatement();
        }
        else if (token.text == u"do") {
          return ParseDoWhileStatement();
        }
        else if (token.text == u"while")
          return ParseWhileStatement();
        else if (token.text == u"for")
          return ParseForStatement();
        else if (token.text == u"continue")
          return ParseContinueStatement();
        else if (token.text == u"break")
          return ParseBreakStatement();
        else if (token.text == u"return")
          return ParseReturnStatement();
        else if (token.text == u"with")
          return ParseWithStatement();
        else if (token.text == u"switch")
          return ParseSwitchStatement();
        else if (token.text == u"throw")
          return ParseThrowStatement();
        else if (token.text == u"try")
          return ParseTryStatement();
        else if (token.text == u"debugger") {
          lexer.next();
          if (!lexer.try_skip_semicolon()) {
            lexer.next();
            goto error;
          }
          return new AST(AST::AST_STMT_DEBUG, SOURCE_PARSED_EXPR);
        }
        break;
      }
      case TokenType::STRICT_FUTURE_KW:
      case TokenType::IDENTIFIER: {
        lexer.checkpoint();
        lexer.next();
        Token colon = lexer.next();
        lexer.rewind();
        if (colon.type == TokenType::COLON)
          return ParseLabelledStatement();
      }
      default:
        break;
    }
    return ParseExpressionStatement();
error:
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseBlockStatement() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_BRACE);
    Block* block = new Block();
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACE) {
      AST* stmt = ParseStatement();
      if (stmt->IsIllegal()) {
        delete block;
        return stmt;
      }
      block->AddStatement(stmt);
      token = lexer.peek();
    }
    assert(token.type == TokenType::RIGHT_BRACE);
    lexer.next();
    lexer.next();
    block->SetSource(SOURCE_PARSED_EXPR);
    return block;
  }

  AST* ParseVariableDeclaration(bool no_in) {
    START_POS;
    Token& const id = lexer.current();
    
    assert(id.is_identifier());
    if (lexer.next().type != TokenType::ASSIGN) {
      VarDecl* var_decl = new VarDecl(id, SOURCE_PARSED_EXPR);
      var_decl_stack.top().emplace_back(var_decl);
      return var_decl;
    }
    // now `curr_token` is ASSIGN
    // in the original version, now `curr_token` is identifier
    AST* init = ParseAssignmentExpression(no_in);
    if (init->IsIllegal()) return init;
    
    VarDecl* var_decl =  new VarDecl(id, init, SOURCE_PARSED_EXPR);
    var_decl_stack.top().emplace_back(var_decl);
    return var_decl;
  }

  AST* ParseVariableStatement(bool no_in) {
    START_POS;
    assert(lexer.current().text == u"var");
    VarStmt* var_stmt = new VarStmt();
    AST* decl;
    Token& const token = lexer.next();
    if (!token.is_identifier()) {
      goto error;
    }
    // Similar to ParseExpression
    decl = ParseVariableDeclaration(no_in);
    if (decl->IsIllegal()) {
      delete var_stmt;
      return decl;
    }
    var_stmt->AddDecl(decl);
    token = lexer.peek();
    while (token.type == TokenType::COMMA) {
      lexer.next();  // skip ,
      decl = ParseVariableDeclaration(no_in);
      if (decl->IsIllegal()) {
        delete var_stmt;
        return decl;
      }
      var_stmt->AddDecl(decl);
      token = lexer.peek();
    }
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      goto error;
    }

    var_stmt->SetSource(SOURCE_PARSED_EXPR);
    return var_stmt;
error:
    delete var_stmt;
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseExpressionStatement() {
    START_POS;
    Token token = lexer.peek();
    if (token.text == u"function") {
      lexer.next();
      return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    assert(token.type != TokenType::LEFT_BRACE);
    AST* exp = ParseExpression(false);
    if (exp->IsIllegal())
      return exp;
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      delete exp;
      return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    return exp;
  }

  AST* ParseIfStatement() {
    START_POS;
    AST* cond;
    AST* if_block;

    assert(lexer.next().text == u"if");
    lexer.next();   // skip (
    cond = ParseExpression(false);
    if (cond->IsIllegal())
      return cond;
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      delete cond;
      goto error;
    }
    if_block = ParseStatement();
    if (if_block->IsIllegal()) {
      delete cond;
      return if_block;
    }
    if (lexer.peek().text == u"else") {
      lexer.next();  // skip else
      AST* else_block = ParseStatement();
      if (else_block->IsIllegal()) {
        delete cond;
        delete if_block;
        return else_block;
      }
      return new If(cond, if_block, else_block, SOURCE_PARSED_EXPR);
    }
    return new If(cond, if_block, SOURCE_PARSED_EXPR);
    
error:
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseDoWhileStatement() {
    START_POS;
    assert(lexer.next().text == u"do");
    AST* cond;
    AST* loop_block;
    loop_block = ParseStatement();
    if (loop_block->IsIllegal()) {
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
    cond = ParseExpression(false);
    if (cond->IsIllegal()) {
      delete loop_block;
      return cond;
    }
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      delete cond;
      goto error;
    }
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      delete cond;
      delete loop_block;
      goto error;
    }
    return new DoWhile(cond, loop_block, SOURCE_PARSED_EXPR);
error:
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseWhileStatement() {
    return ParseWhileOrWithStatement(u"while", AST::AST_STMT_WHILE);
  }

  AST* ParseWithStatement() {
    return ParseWhileOrWithStatement(u"with", AST::AST_STMT_WITH);
  }

  AST* ParseWhileOrWithStatement(std::u16string keyword, AST::Type type) {
    START_POS;
    assert(lexer.next().text == keyword);
    AST* expr;
    AST* stmt;
    if (lexer.next().type != TokenType::LEFT_PAREN) { // skip (
      goto error;
    }
    expr = ParseExpression(false);
    if (expr->IsIllegal())
      return expr;
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      delete expr;
      goto error;
    }
    stmt = ParseStatement();
    if (stmt->IsIllegal()) {
      delete expr;
      return stmt;
    }
    return new WhileOrWith(type, expr, stmt, SOURCE_PARSED_EXPR);
error:
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseForStatement() {
    START_POS;
    assert(lexer.next().text == u"for");
    Token token = lexer.next();  // skip (
    AST* expr0;
    if (token.type != TokenType::LEFT_PAREN)
      goto error;
    token = lexer.peek();
    if (token.is_semicolon()) {
      return ParseForStatement({}, start);  // for (;
    } else if (token.text == u"var") {
      lexer.next();  // skip var
      std::vector<AST*> expr0s;

      // NOTE(zhuzilin) the starting token for ParseVariableDeclaration
      // must be identifier. This is for better error code.
      if (!lexer.peek().is_identifier()) {
        goto error;
      }
      expr0 = ParseVariableDeclaration(true);
      if (expr0->IsIllegal())
        return expr0;

      token = lexer.peek();
      if (token.text == u"in")  // var VariableDeclarationNoIn in
        return ParseForInStatement(expr0, start);

      expr0s.emplace_back(expr0);
      while (!token.is_semicolon()) {
        // NOTE(zhuzilin) the starting token for ParseVariableDeclaration
        // must be identifier. This is for better error code.
        if (lexer.next().type != TokenType::COMMA ||  // skip ,
            !lexer.peek().is_identifier()) {
          for (auto expr : expr0s)
            delete expr;
          goto error;
        }

        expr0 = ParseVariableDeclaration(true);
        if (expr0->IsIllegal()) {
          for (auto expr : expr0s)
            delete expr;
          return expr0;
        }
        expr0s.emplace_back(expr0);
        token = lexer.peek();
      }
      return ParseForStatement(expr0s, start);  // var VariableDeclarationListNoIn;
    } else {
      expr0 = ParseExpression(true);
      if (expr0->IsIllegal()) {
        return expr0;
      }
      token = lexer.peek();
      if (token.is_semicolon()) {
        return ParseForStatement({expr0}, start);  // for ( ExpressionNoIn;
      } else if (token.text == u"in" &&
                 expr0->type() == AST::AST_EXPR_LHS) {  // for ( LeftHandSideExpression in
        return ParseForInStatement(expr0, start);  
      } else {
        delete expr0;
        goto error;
      }
    }
error:
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseForStatement(std::vector<AST*> expr0s, u32 start) {
    assert(lexer.next().is_semicolon());
    AST* expr1 = nullptr;
    AST* expr2 = nullptr;
    AST* stmt;
    Token token = lexer.peek();
    if (!token.is_semicolon()) {
      expr1 = ParseExpression(false);  // for (xxx; Expression
      if (expr1->IsIllegal()) {
        for (auto expr : expr0s) {
          delete expr;
        }
        return expr1;
      }
    }

    if (!lexer.next().is_semicolon()) {  // skip ;
      lexer.next();
      goto error;
    }

    token = lexer.peek();
    if (token.type != TokenType::RIGHT_PAREN) {
      expr2 = ParseExpression(false);  // for (xxx; xxx; Expression
      if (expr2->IsIllegal()) {
        for (auto expr : expr0s) {
          delete expr;
        }
        if (expr1 != nullptr)
          delete expr1;
        return expr2;
      }
    }

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      lexer.next();
      goto error;
    }

    stmt = ParseStatement();
    if (stmt->IsIllegal()) {
      for (auto expr : expr0s) {
        delete expr;
      }
      if (expr1 != nullptr)
        delete expr1;
      if (expr2 != nullptr)
        delete expr2;
      return stmt;
    }

    return new For(expr0s, expr1, expr2, stmt, SOURCE_PARSED_EXPR);
error:
    for (auto expr : expr0s) {
      delete expr;
    }
    if (expr1 != nullptr)
      delete expr1;
    if (expr2 != nullptr)
      delete expr2;
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseForInStatement(AST* expr0, u32 start) {
    assert(lexer.next().text == u"in");
    AST* expr1 = ParseExpression(false);  // for ( xxx in Expression
    AST* stmt;
    if (expr1->IsIllegal()) {
      delete expr0;
      return expr1;
    }

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      lexer.next();
      goto error;
    }

    stmt = ParseStatement();
    if (stmt->IsIllegal()) {
      delete expr0;
      delete expr1;
      return stmt;
    }
    return new ForIn(expr0, expr1, stmt, SOURCE_PARSED_EXPR);
error:
    delete expr0;
    delete expr1;
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseContinueStatement() {
    return ParseContinueOrBreakStatement(u"continue", AST::AST_STMT_CONTINUE);
  }

  AST* ParseBreakStatement() {
    return ParseContinueOrBreakStatement(u"break", AST::AST_STMT_BREAK);
  }

  AST* ParseContinueOrBreakStatement(std::u16string keyword, AST::Type type) {
    START_POS;
    assert(lexer.next().text == keyword);
    if (!lexer.try_skip_semicolon()) {
      Token ident = lexer.peek();
      if (ident.is_identifier()) {
        lexer.next();  // Skip Identifier
      }
      if (!lexer.try_skip_semicolon()) {
        lexer.next();
        return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
      return new ContinueOrBreak(type, ident, SOURCE_PARSED_EXPR);
    }
    return new ContinueOrBreak(type, SOURCE_PARSED_EXPR);
  }

  AST* ParseReturnStatement() {
    START_POS;
    assert(lexer.next().text == u"return");
    AST* expr = nullptr;
    if (!lexer.try_skip_semicolon()) {
      expr = ParseExpression(false);
      if (expr->IsIllegal()) {
        return expr;
      }
      if (!lexer.try_skip_semicolon()) {
        lexer.next();
        delete expr;
        return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
    }
    return new Return(expr, SOURCE_PARSED_EXPR);
  }

  AST* ParseThrowStatement() {
    START_POS;
    assert(lexer.next().text == u"throw");
    AST* expr = nullptr;
    if (!lexer.try_skip_semicolon()) {
      expr = ParseExpression(false);
      if (expr->IsIllegal()) {
        return expr;
      }
      if (!lexer.try_skip_semicolon()) {
        lexer.next();
        delete expr;
        return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
    }
    return new Throw(expr, SOURCE_PARSED_EXPR);
  }

  AST* ParseSwitchStatement() {
    START_POS;
    Switch* switch_stmt = new Switch();
    AST* expr;
    Token token = lexer.current();
    assert(lexer.next().text == u"switch");
    if (lexer.next().type != TokenType::LEFT_PAREN) { // skip (
      goto error;
    }
    expr = ParseExpression(false);
    if (expr->IsIllegal()) {
      delete switch_stmt;
      return expr;
    }
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      delete expr;
      goto error;
    }
    switch_stmt->SetExpr(expr);
    if (lexer.next().type != TokenType::LEFT_BRACE) { // skip {
      goto error;
    }
    // Loop for parsing CaseClause
    token = lexer.peek();
    while (token.type != TokenType::RIGHT_BRACE) {
      AST* case_expr = nullptr;
      std::vector<AST*> stmts;
      std::u16string_view type = token.text;
      if (type == u"case") {
        lexer.next();  // skip case
        case_expr = ParseExpression(false);
        if (case_expr->IsIllegal()) {
          delete switch_stmt;
          return case_expr;
        }
      } else if (type == u"default") {
        lexer.next();  // skip default
        // can only have one default.
        if (switch_stmt->has_default_clause())
          goto error;
      } else {
        lexer.next();
        goto error;
      }
      if (lexer.next().type != TokenType::COLON) { // skip :
        delete case_expr;
        goto error;
      }
      // parse StatementList
      token = lexer.peek();
      while (token.text != u"case" && token.text != u"default" &&
             token.type != TokenType::RIGHT_BRACE) {
        AST* stmt = ParseStatement();
        if (stmt->IsIllegal()) {
          for (auto s : stmts) {
            delete s;
          }
          delete switch_stmt;
          return stmt;
        }
        stmts.emplace_back(stmt);
        token = lexer.peek();
      }
      if (type == u"case") {
        if (switch_stmt->has_default_clause()) {
          switch_stmt->AddAfterDefaultCaseClause(Switch::CaseClause(case_expr, stmts));
        } else {
          switch_stmt->AddBeforeDefaultCaseClause(Switch::CaseClause(case_expr, stmts));
        }
      } else {
        switch_stmt->SetDefaultClause(stmts);
      }
      token = lexer.peek();
    }
    assert(token.type == TokenType::RIGHT_BRACE);
    assert(lexer.next().type == TokenType::RIGHT_BRACE);
    switch_stmt->SetSource(SOURCE_PARSED_EXPR);
    return switch_stmt;
error:
    delete switch_stmt;
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseTryStatement() {
    START_POS;
    assert(lexer.next().text == u"try");

    AST* try_block = nullptr;
    Token catch_ident(TokenType::NONE, source, 0, 0);
    AST* catch_block = nullptr;
    AST* finally_block = nullptr;

    if (lexer.peek().type != TokenType::LEFT_BRACE) {
      lexer.next();
      goto error;
    }
    try_block = ParseBlockStatement();
    if (try_block->IsIllegal())
      return try_block;
    if (lexer.peek().text == u"catch") {
      lexer.next();  // skip catch
      if (lexer.next().type != TokenType::LEFT_PAREN) {  // skip (
        delete try_block;
        goto error;
      }
      catch_ident = lexer.next();  // skip identifier
      if (!catch_ident.is_identifier()) {
        goto error;
      }
      if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
        delete try_block;
        goto error;
      }
      catch_block = ParseBlockStatement();
      if (catch_block->IsIllegal()) {
        delete try_block;
        return catch_block;
      }
    }
    if (lexer.peek().text == u"finally") {
      lexer.next();  // skip finally
      if (lexer.peek().type != TokenType::LEFT_BRACE) {
        lexer.next();
        goto error;
      }
      finally_block = ParseBlockStatement();
      if (finally_block->IsIllegal()) {
        delete try_block;
        if (catch_block != nullptr)
          delete catch_block;
        return finally_block;
      }
    }
    if (catch_block == nullptr && finally_block == nullptr) {
      goto error;
    } else if (finally_block == nullptr) {
      assert(catch_block != nullptr && catch_ident.is_identifier());
      return new Try(try_block, catch_ident, catch_block, SOURCE_PARSED_EXPR);
    } else if (catch_block == nullptr) {
      assert(finally_block != nullptr);
      return new Try(try_block, finally_block, SOURCE_PARSED_EXPR);
    }
    assert(catch_block != nullptr && catch_ident.is_identifier());
    assert(finally_block != nullptr);
    return new Try(try_block, catch_ident, catch_block, finally_block, SOURCE_PARSED_EXPR);
error:
    if (try_block != nullptr)
      delete try_block;
    if (catch_block != nullptr)
      delete try_block;
    if (finally_block != nullptr)
      delete finally_block;
    return new AST(AST::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  AST* ParseLabelledStatement() {
    START_POS;
    Token ident = lexer.next();  // skip identifier
    assert(lexer.next().type == TokenType::COLON);  // skip colon
    AST* stmt = ParseStatement();
    if (stmt->IsIllegal()) {
      return stmt;
    }
    return new LabelledStmt(ident, stmt, SOURCE_PARSED_EXPR);
  }

 private:
  std::u16string source;
  Lexer lexer;

  std::stack<std::vector<VarDecl*>> var_decl_stack;
};

}  // namespace njs

#endif  // NJS_PARSER_PARSER_H