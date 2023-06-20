#ifndef NJS_PARSER_H
#define NJS_PARSER_H

#include <stack>

#include "njs/parser/lexer.h"
#include "njs/parser/ast.h"
#include "njs/utils/helper.h"

#define START_POS u32 start = lexer.current().start, line_start = lexer.current().line
#define SOURCE_PARSED_EXPR std::u16string_view(source.data() + start, lexer.current_pos() - start), \
                            start, lexer.current_pos(), line_start

#define TOKEN_SOURCE_EXPR token.text, token.start, token.end, token.line

namespace njs {

class Parser {
 public:
  Parser(std::u16string source) : source(source), lexer(source) {
    // For test
    var_decl_stack.push({});
  }

  ASTNode* ParsePrimaryExpression() {
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
        return new ASTNode(ASTNode::AST_EXPR_IDENT, TOKEN_SOURCE_EXPR);
      case TokenType::TK_NULL:
        return new ASTNode(ASTNode::AST_EXPR_NULL, TOKEN_SOURCE_EXPR);
      case TokenType::TK_BOOL:
        return new ASTNode(ASTNode::AST_EXPR_BOOL, TOKEN_SOURCE_EXPR);
      case TokenType::NUMBER:
        return new ASTNode(ASTNode::AST_EXPR_NUMBER, TOKEN_SOURCE_EXPR);
      case TokenType::STRING:
        return new ASTNode(ASTNode::AST_EXPR_STRING, TOKEN_SOURCE_EXPR);
      case TokenType::LEFT_BRACK:  // [
        return ParseArrayLiteral();
      case TokenType::LEFT_BRACE:  // {
        return ParseObjectLiteral();
      case TokenType::LEFT_PAREN: { // (
        lexer.next();   // skip (
        ASTNode* value = ParseExpression(false);
        if (value->is_illegal()) return value;
        if (lexer.next().type != TokenType::RIGHT_PAREN) {
          delete value;
          goto error;
        }
        return new ParenthesisExpr(value, value->source(),value->start_pos(), value->end_pos(),
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

  ASTNode* ParseFunction(bool name_required, bool func_keyword_required = true) {
    START_POS;
    if (func_keyword_required) {
      assert(lexer.current().text == u"function");
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
      lexer.next();
    }
    else if (name_required) {
      goto error;
    }
    if (lexer.current().type != TokenType::LEFT_PAREN) {
      goto error;
    }
    lexer.next();
    if (lexer.current().is_identifier()) {
      if (!ParseFormalParameterList(params)) {
        goto error;
      }
    }
    if (lexer.current().type != TokenType::RIGHT_PAREN) {
      goto error;
    }
    if (lexer.next().type != TokenType::LEFT_BRACE) {
      goto error;
    }
    lexer.next();
    body = ParseFunctionBody();
    if (body->is_illegal())
      return body;

    if (lexer.current().type != TokenType::RIGHT_BRACE) {
      goto error;
    }

    return new Function(name, params, body, SOURCE_PARSED_EXPR);
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseArrayLiteral() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_BRACK);

    ArrayLiteral* array = new ArrayLiteral();
    ASTNode* element = nullptr;

    // get the token after `[`
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACK) {
      switch (token.type) {
        case TokenType::COMMA:
          array->add_element(element);
          element = nullptr;
          break;
        default:
          element = ParseAssignmentExpression(false);
          if (element->get_type() == ASTNode::AST_ILLEGAL) {
            delete array;
            return element;
          }
      }
      token = lexer.next();
    }
    if (element != nullptr) {
      array->add_element(element);
    }
    assert(token.type == TokenType::RIGHT_BRACK);
    array->set_source(SOURCE_PARSED_EXPR);
    return array;
  }

  ASTNode* ParseObjectLiteral() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_BRACE);

    ObjectLiteral* obj = new ObjectLiteral();
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACE) {
      if (token.is_property_name()) {
        // getter or setter
        if ((token.text == u"get" || token.text == u"set") && lexer.peek().is_property_name()) {
          START_POS;

          ObjectLiteral::Property::Type type 
            = token.text == u"get" ? ObjectLiteral::Property::GET : ObjectLiteral::Property::SET;

          Token key = lexer.next();  // skip property name
          if (!key.is_property_name()) {
            goto error;
          }

          ASTNode* get_set_func = ParseFunction(true, false);
          if (get_set_func->is_illegal()) {
            delete get_set_func;
            goto error;
          }

          obj->set_property(ObjectLiteral::Property(key, get_set_func, type));
        }
        else {
          if (lexer.next().type != TokenType::COLON) goto error;
          
          lexer.next();
          ASTNode* value = ParseAssignmentExpression(false);
          if (value->get_type() == ASTNode::AST_ILLEGAL) goto error;
          obj->set_property(ObjectLiteral::Property(token, value, ObjectLiteral::Property::NORMAL));
        }
      }
      else {
        goto error;
      }
      token = lexer.next();
      if (lexer.current().type == TokenType::COMMA) {
        token = lexer.next();  // Skip ,
      }
    }
    assert(token.type == TokenType::RIGHT_BRACE);
    obj->set_source(SOURCE_PARSED_EXPR);
    return obj;
error:
    delete obj;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseExpression(bool no_in) {
    START_POS;

    ASTNode* element = ParseAssignmentExpression(no_in);
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
      element = ParseAssignmentExpression(no_in);
      if (element->is_illegal()) {
        delete expr;
        return element;
      }
      expr->add_element(element);
    }
    expr->set_source(SOURCE_PARSED_EXPR);
    return expr;
  }

  ASTNode* ParseAssignmentExpression(bool no_in) {
    START_POS;

    ASTNode* lhs = ParseConditionalExpression(no_in);
    if (lhs->is_illegal())
      return lhs;

    // Not LeftHandSideExpression
    if (lhs->get_type() != ASTNode::AST_EXPR_LHS) {
      return lhs;
    }
    
    Token op = lexer.peek();
    if (!op.is_assignment_operator()) {
      return lhs;
    }
    
    lexer.next();
    lexer.next();
    ASTNode* rhs = ParseAssignmentExpression(no_in);
    if (rhs->is_illegal()) {
      delete lhs;
      return rhs;
    }

    return new BinaryExpr(lhs, rhs, op, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseConditionalExpression(bool no_in) {
    START_POS;
    ASTNode* cond = ParseBinaryAndUnaryExpression(no_in, 0);
    if (cond->is_illegal())
      return cond;

    if (lexer.peek().type != TokenType::QUESTION) {
      return cond;
    }
      
    lexer.next();
    lexer.next();
    ASTNode* lhs = ParseAssignmentExpression(no_in);
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
    ASTNode* rhs = ParseAssignmentExpression(no_in);
    if (lhs->is_illegal()) {
      delete cond;
      delete lhs;
      return rhs;
    }
    ASTNode* triple = new TernaryExpr(cond, lhs, rhs);
    triple->set_source(SOURCE_PARSED_EXPR);
    return triple;
  }

  ASTNode* ParseBinaryAndUnaryExpression(bool no_in, int priority) {
    START_POS;
    ASTNode* lhs = nullptr;
    ASTNode* rhs = nullptr;
    // Prefix Operators.
    Token prefix_op = lexer.current();
    // NOTE(zhuzilin) !!a = !(!a)
    if (prefix_op.unary_priority() >= priority) {
      lexer.next();
      lhs = ParseBinaryAndUnaryExpression(no_in, prefix_op.unary_priority());
      if (lhs->is_illegal())
        return lhs;
      lhs = new UnaryExpr(lhs, prefix_op, true);
    }
    else {
      lhs = ParseLeftHandSideExpression();
      if (lhs->is_illegal()) return lhs;
      // Postfix Operators.
      //
      // Because the priority of postfix operators are higher than prefix ones,
      // they won't be parsed at the same time.
      
      const Token& postfix_op = lexer.peek();
      if (!lexer.line_term_ahead() && postfix_op.postfix_priority() > priority) {
        lexer.next();
        if (lhs->get_type() != ASTNode::AST_EXPR_BINARY && lhs->get_type() != ASTNode::AST_EXPR_UNARY) {
          lhs = new UnaryExpr(lhs, postfix_op, false);
          lhs->set_source(SOURCE_PARSED_EXPR);
        }
        else {
          delete lhs;
          return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
        }
      }
    }

    while (true) {
      
      Token binary_op = lexer.peek();
      if (binary_op.binary_priority(no_in) > priority) {
        
        lexer.next();
        lexer.next();
        rhs = ParseBinaryAndUnaryExpression(no_in, binary_op.binary_priority(no_in));
        if (rhs->is_illegal())
          return rhs;
        lhs = new BinaryExpr(lhs, rhs, binary_op, SOURCE_PARSED_EXPR);
      }
      else {
        // TODO: make sure this is correct.
        break;
      }
    }
    
    lhs->set_source(SOURCE_PARSED_EXPR);
    return lhs;
  }

  ASTNode* ParseLeftHandSideExpression() {
    START_POS;
    Token token = lexer.current();
    ASTNode* base;
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
    if (base->is_illegal()) {
      return base;
    }
    LeftHandSideExpr* lhs = new LeftHandSideExpr(base, new_count);

    while (true) {
      
      token = lexer.peek();
      switch (token.type) {
        case TokenType::LEFT_PAREN: {  // (
          lexer.next();
          ASTNode* ast = ParseArguments();
          if (ast->is_illegal()) {
            delete lhs;
            return ast;
          }
          assert(ast->get_type() == ASTNode::AST_EXPR_ARGS);
          Arguments* args = static_cast<Arguments*>(ast);
          lhs->AddArguments(args);
          break;
        }
        case TokenType::LEFT_BRACK: {  // [
          lexer.next();  // skip [
          lexer.next();
          ASTNode* index = ParseExpression(false);
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
          lhs->AddIndex(index);
          break;
        }
        case TokenType::DOT: {  // .
          lexer.next();
          token = lexer.next();  // read identifier name
          if (!token.is_identifier_name()) {
            delete lhs;
            goto error;
          }
          lhs->AddProp(token);
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

  ASTNode* ParseArguments() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_PAREN);
    std::vector<ASTNode*> args;
    ASTNode* arg;
    Arguments* arg_ast;
    lexer.next();
    if (lexer.current().type != TokenType::RIGHT_PAREN) {
      arg = ParseAssignmentExpression(false);
      if (arg->is_illegal())
        return arg;
      args.emplace_back(arg);
      lexer.next();
    }
    while (lexer.current().type != TokenType::RIGHT_PAREN) {
      if (lexer.current().type != TokenType::COMMA) {
        goto error;
      }
      lexer.next();
      arg = ParseAssignmentExpression(false);
      if (arg->is_illegal()) {
        for (auto arg : args)
          delete arg;
        return arg;
      }
      args.emplace_back(arg);
      lexer.next();
    }
    assert(lexer.current().type == TokenType::RIGHT_PAREN);
    arg_ast = new Arguments(args);
    arg_ast->set_source(SOURCE_PARSED_EXPR);
    return arg_ast;
error:
    for (auto arg : args)
      delete arg;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseFunctionBody(TokenType ending_token_type = TokenType::RIGHT_BRACE) {
    return ParseProgramOrFunctionBody(ending_token_type, ASTNode::AST_FUNC_BODY);
  }

  ASTNode* ParseProgram() {
    lexer.next();
    return ParseProgramOrFunctionBody(TokenType::EOS, ASTNode::AST_PROGRAM);
  }

  ASTNode* ParseProgramOrFunctionBody(TokenType ending_token_type, ASTNode::Type syntax_type) {
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

    ProgramOrFunctionBody* prog = new ProgramOrFunctionBody(syntax_type, strict);
    var_decl_stack.push({});

    ASTNode* element;
    
    while (lexer.current().type != ending_token_type) {
      if (lexer.current().text == u"function") {
        element = ParseFunction(true);
        if (element->is_illegal()) {
          delete prog;
          return element;
        }
        prog->add_function_decl(element);
      }
      else {
        element = ParseStatement();
        if (element->is_illegal()) {
          delete prog;
          return element;
        }
        prog->add_statement(element);
      }
      lexer.next();
    }
    assert(lexer.current().type == ending_token_type);
    prog->set_var_decls(std::move(var_decl_stack.top()));
    var_decl_stack.pop();
    prog->set_source(SOURCE_PARSED_EXPR);
    return prog;
  }

  ASTNode* ParseStatement() {
    START_POS;
    const Token& token = lexer.current();

    switch (token.type) {
      case TokenType::LEFT_BRACE:  // {
        return ParseBlockStatement();
      case TokenType::SEMICOLON:  // ;
        return new ASTNode(ASTNode::AST_STMT_EMPTY, TOKEN_SOURCE_EXPR);
      case TokenType::KEYWORD: {
        if (token.text == u"var") return ParseVariableStatement(false);
        else if (token.text == u"if") return ParseIfStatement();
        else if (token.text == u"do") return ParseDoWhileStatement();
        else if (token.text == u"while") return ParseWhileStatement();
        else if (token.text == u"for") return ParseForStatement();
        else if (token.text == u"continue") return ParseContinueStatement();
        else if (token.text == u"break") return ParseBreakStatement();
        else if (token.text == u"return") return ParseReturnStatement();
        else if (token.text == u"with") return ParseWithStatement();
        else if (token.text == u"switch") return ParseSwitchStatement();
        else if (token.text == u"throw") return ParseThrowStatement();
        else if (token.text == u"try") return ParseTryStatement();
        else if (token.text == u"function") return ParseFunction(true, true);
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
          return ParseLabelledStatement();
        }
      }
      default:
        break;
    }
    return ParseExpressionStatement();
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseBlockStatement() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_BRACE);
    Block* block = new Block();
    lexer.next();
    while (lexer.current().type != TokenType::RIGHT_BRACE) {
      ASTNode* stmt = ParseStatement();
      if (stmt->is_illegal()) {
        delete block;
        return stmt;
      }
      block->add_statement(stmt);
      lexer.next();
    }
    assert(lexer.current().type == TokenType::RIGHT_BRACE);
    block->set_source(SOURCE_PARSED_EXPR);
    return block;
  }

  ASTNode* ParseVariableDeclaration(bool no_in) {
    START_POS;
    Token id = lexer.current();
    
    assert(id.is_identifier());

    if (lexer.peek().type != TokenType::ASSIGN) {
      VarDecl* var_decl = new VarDecl(id, SOURCE_PARSED_EXPR);
      var_decl_stack.top().emplace_back(var_decl);
      return var_decl;
    }
    
    ASTNode* init = ParseAssignmentExpression(no_in);
    if (init->is_illegal()) return init;
    
    VarDecl* var_decl =  new VarDecl(id, init, SOURCE_PARSED_EXPR);
    var_decl_stack.top().emplace_back(var_decl);
    return var_decl;
  }

  ASTNode* ParseVariableStatement(bool no_in) {
    START_POS;
    assert(lexer.current().text == u"var");
    VarStatement* var_stmt = new VarStatement();
    ASTNode* decl;
    if (!lexer.next().is_identifier()) {
      goto error;
    }
    // Similar to ParseExpression
    decl = ParseVariableDeclaration(no_in);
    if (decl->is_illegal()) {
      delete var_stmt;
      return decl;
    }
    var_stmt->add_decl(decl);

    while (!lexer.try_skip_semicolon()) {
      if (lexer.next().text != u",") goto error;
      lexer.next();
      decl = ParseVariableDeclaration(no_in);
      if (decl->is_illegal()) {
        delete var_stmt;
        return decl;
      }
      var_stmt->add_decl(decl);
    }

    var_stmt->set_source(SOURCE_PARSED_EXPR);
    return var_stmt;
error:
    delete var_stmt;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseExpressionStatement() {
    START_POS;
    const Token& token = lexer.current();
    if (token.text == u"function") {
      return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    // already handled LEFT_BRACE case in caller.
    assert(token.type != TokenType::LEFT_BRACE);
    ASTNode* exp = ParseExpression(false);
    if (exp->is_illegal())
      return exp;
    if (!lexer.try_skip_semicolon()) {
      delete exp;
      return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    return exp;
  }

  ASTNode* ParseIfStatement() {
    START_POS;
    ASTNode* cond;
    ASTNode* if_block;

    assert(lexer.current().text == u"if");
    lexer.next();
    lexer.next();
    cond = ParseExpression(false);
    if (cond->is_illegal())
      return cond;
    if (lexer.next().type != TokenType::RIGHT_PAREN) {
      delete cond;
      goto error;
    }
    lexer.next();
    if_block = ParseStatement();
    if (if_block->is_illegal()) {
      delete cond;
      return if_block;
    }
    
    if (lexer.peek().text == u"else") {
      lexer.next();
      lexer.next();
      ASTNode* else_block = ParseStatement();
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

  ASTNode* ParseDoWhileStatement() {
    START_POS;
    assert(lexer.current().text == u"do");
    ASTNode* cond;
    ASTNode* loop_block;
    lexer.next();
    loop_block = ParseStatement();
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
    cond = ParseExpression(false);
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

  ASTNode* ParseWhileStatement() {
    return ParseWhileOrWithStatement(ASTNode::AST_STMT_WHILE);
  }

  ASTNode* ParseWithStatement() {
    return ParseWhileOrWithStatement(ASTNode::AST_STMT_WITH);
  }

  ASTNode* ParseWhileOrWithStatement(ASTNode::Type type) {
    START_POS;
    std::u16string keyword = type == ASTNode::AST_STMT_WHILE ? u"while" : u"with";
    assert(lexer.current().text == keyword);
    ASTNode* expr;
    ASTNode* stmt;
    if (lexer.next().type != TokenType::LEFT_PAREN) { // read (
      goto error;
    }
    lexer.next();
    expr = ParseExpression(false);
    if (expr->is_illegal())
      return expr;
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // read )
      delete expr;
      goto error;
    }
    lexer.next();
    stmt = ParseStatement();
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

  ASTNode* ParseForStatement() {
    START_POS;
    assert(lexer.current().text == u"for");

    ASTNode* expr0;
    if (lexer.next().type != TokenType::LEFT_PAREN) goto error;

    lexer.next();
    if (lexer.current().is_semicolon()) {
      return ParseForStatement({}, start, line_start);  // for (;
    }
    else if (lexer.current().text == u"var") {
      lexer.next();  // skip var
      std::vector<ASTNode*> expr0s;

      // NOTE(zhuzilin) the starting token for ParseVariableDeclaration
      // must be identifier. This is for better error code.
      if (!lexer.current().is_identifier()) {
        goto error;
      }
      expr0 = ParseVariableDeclaration(true);
      if (expr0->is_illegal())
        return expr0;

      // expect `in`, `,`
      // the `in` case
      if (lexer.next().text == u"in")  // var VariableDeclarationNoIn in
        return ParseForInStatement(expr0, start, line_start);

      // the `,` case
      expr0s.emplace_back(expr0);
      while (!lexer.current().is_semicolon()) {
        // NOTE(zhuzilin) the starting token for ParseVariableDeclaration
        // must be identifier. This is for better error code.
        if (lexer.current().type != TokenType::COMMA || !lexer.next().is_identifier()) {
          for (auto expr : expr0s)
            delete expr;
          goto error;
        }

        expr0 = ParseVariableDeclaration(true);
        if (expr0->is_illegal()) {
          for (auto expr : expr0s)
            delete expr;
          return expr0;
        }
        expr0s.emplace_back(expr0);
        lexer.next();
      }
      return ParseForStatement(expr0s, start, line_start);  // for (var VariableDeclarationListNoIn; ...)
    }
    else {
      expr0 = ParseExpression(true);
      if (expr0->is_illegal()) {
        return expr0;
      }
      lexer.next();
      if (lexer.current().is_semicolon()) {
        return ParseForStatement({expr0}, start, line_start);  // for ( ExpressionNoIn;
      }
      // for ( LeftHandSideExpression in
      else if (lexer.current().text == u"in" && expr0->get_type() == ASTNode::AST_EXPR_LHS) {
        return ParseForInStatement(expr0, start, line_start);  
      }
      else {
        delete expr0;
        goto error;
      }
    }
error:
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseForStatement(std::vector<ASTNode*> expr0s, u32 start, u32 line_start) {
    assert(lexer.current().is_semicolon());
    ASTNode* expr1 = nullptr;
    ASTNode* expr2 = nullptr;
    ASTNode* stmt;

    if (!lexer.peek().is_semicolon()) {
      lexer.next();
      expr1 = ParseExpression(false);  // for (xxx; Expression
      if (expr1->is_illegal()) {
        for (auto expr : expr0s) {
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
      expr2 = ParseExpression(false);  // for (xxx; xxx; Expression
      if (expr2->is_illegal()) {
        for (auto expr : expr0s) {
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
    stmt = ParseStatement();
    if (stmt->is_illegal()) {
      for (auto expr : expr0s) {
        delete expr;
      }
      if (expr1 != nullptr)
        delete expr1;
      if (expr2 != nullptr)
        delete expr2;
      return stmt;
    }

    return new ForStatement(expr0s, expr1, expr2, stmt, SOURCE_PARSED_EXPR);
error:
    for (auto expr : expr0s) {
      delete expr;
    }
    if (expr1 != nullptr)
      delete expr1;
    if (expr2 != nullptr)
      delete expr2;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseForInStatement(ASTNode* expr0, u32 start, u32 line_start) {
    assert(lexer.current().text == u"in");
    lexer.next();
    ASTNode* expr1 = ParseExpression(false);  // for ( xxx in Expression
    ASTNode* stmt;
    if (expr1->is_illegal()) {
      delete expr0;
      return expr1;
    }

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      goto error;
    }

    lexer.next();
    stmt = ParseStatement();
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

  ASTNode* ParseContinueStatement() {
    return ParseContinueOrBreakStatement(u"continue", ASTNode::AST_STMT_CONTINUE);
  }

  ASTNode* ParseBreakStatement() {
    return ParseContinueOrBreakStatement(u"break", ASTNode::AST_STMT_BREAK);
  }

  ASTNode* ParseContinueOrBreakStatement(std::u16string_view keyword, ASTNode::Type type) {
    START_POS;
    assert(lexer.current().text == keyword);
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

  ASTNode* ParseReturnStatement() {
    START_POS;
    assert(lexer.current().text == u"return");
    ASTNode* expr = nullptr;
    
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      expr = ParseExpression(false);
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

  ASTNode* ParseThrowStatement() {
    START_POS;
    assert(lexer.current().text == u"throw");
    ASTNode* expr = nullptr;
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      expr = ParseExpression(false);
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

  ASTNode* ParseSwitchStatement() {
    START_POS;
    SwitchStatement* switch_stmt = new SwitchStatement();
    ASTNode* expr;
    // Token token = lexer.current();
    assert(lexer.current().text == u"switch");
    if (lexer.next().type != TokenType::LEFT_PAREN) { // skip (
      goto error;
    }
    lexer.next();
    expr = ParseExpression(false);
    if (expr->is_illegal()) {
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
    lexer.next();
    while (lexer.current().type != TokenType::RIGHT_BRACE) {
      ASTNode* case_expr = nullptr;
      std::vector<ASTNode*> stmts;
      std::u16string_view type = lexer.current().text;
      if (type == u"case") {
        lexer.next();  // skip case
        case_expr = ParseExpression(false);
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
      while (lexer.current().text != u"case" && lexer.current().text != u"default" &&
              lexer.current().type != TokenType::RIGHT_BRACE) {
        ASTNode* stmt = ParseStatement();
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
          switch_stmt->AddAfterDefaultCaseClause(SwitchStatement::CaseClause(case_expr, stmts));
        }
        else {
          switch_stmt->AddBeforeDefaultCaseClause(SwitchStatement::CaseClause(case_expr, stmts));
        }
      }
      else {
        switch_stmt->SetDefaultClause(stmts);
      }
    }

    assert(lexer.current().type == TokenType::RIGHT_BRACE);
    switch_stmt->set_source(SOURCE_PARSED_EXPR);
    return switch_stmt;
error:
    delete switch_stmt;
    return new ASTNode(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* ParseTryStatement() {
    START_POS;
    assert(lexer.current().text == u"try");

    ASTNode* try_block = nullptr;
    Token catch_id(TokenType::NONE, source, 0, 0);
    ASTNode* catch_block = nullptr;
    ASTNode* finally_block = nullptr;

    if (lexer.next().type != TokenType::LEFT_BRACE) {
      goto error;
    }
    try_block = ParseBlockStatement();
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
      catch_block = ParseBlockStatement();
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
      finally_block = ParseBlockStatement();
      if (finally_block->is_illegal()) {
        delete try_block;
        if (catch_block != nullptr)
          delete catch_block;
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

  ASTNode* ParseLabelledStatement() {
    START_POS;
    Token id = lexer.current();  // skip identifier
    lexer.next();
    assert(lexer.current().type == TokenType::COLON);  // skip colon
    lexer.next();
    ASTNode* stmt = ParseStatement();
    if (stmt->is_illegal()) {
      return stmt;
    }
    return new LabelledStatement(id, stmt, SOURCE_PARSED_EXPR);
  }

 private:
  std::u16string source;
  Lexer lexer;

  std::stack<std::vector<VarDecl*>> var_decl_stack;
};

}  // namespace njs

#endif  // NJS_PARSER_H