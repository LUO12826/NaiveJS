#ifndef NJS_PARSER_H
#define NJS_PARSER_H

#include <ranges>
#include <stack>
#include <iostream>

#include "njs/codegen/SymbolRecord.h"
#include "njs/common/common_types.h"
#include "njs/common/enums.h"
#include "njs/common/JSErrorType.h"
#include "njs/include/SmallVector.h"
#include "njs/parser/ast.h"
#include "njs/parser/Lexer.h"
#include "njs/utils/helper.h"
#include "njs/common/Defer.h"
#include "njs/basic_types/conversion.h"

#define START_POS SourceLoc start = lexer.current().get_src_start();
#define RENEW_START start = lexer.current().get_src_start();
#define SOURCE_PARSED_EXPR u16string_view(source.data() + start.char_idx, lexer.current_pos() - start.char_idx), \
                            start, lexer.current().get_src_end()
#define TOKEN_SOURCE_EXPR token.text, token.get_src_start(), token.get_src_end()

namespace njs {

using std::u16string;
using std::u16string_view;
using std::unique_ptr;
using TokenType = Token::TokenType;

struct ParsingError {
  JSErrorType error_type;
  std::string message;
  SourceLoc start;
  SourceLoc end;

  void describe() {
    printf("At line %u: %s: %s\n",
           start.line,
           to_u8string(native_error_name[error_type]).c_str(),
           message.c_str());
  }
};

class Parser {
 public:
  explicit Parser(u16string src) : source(std::move(src)), lexer(this->source) {}

  ASTNode* parse_primary_expression() {
    Token token = lexer.current();
    switch (token.type) {
      case TokenType::KEYWORD:
        if (token.text == u"this") {
          return new ASTNode(ASTNode::EXPR_THIS, TOKEN_SOURCE_EXPR);
        }
        goto error;
      case TokenType::STRICT_FUTURE_KW:
        if (scope().get_outer_func()->is_strict) {
          return new ASTNode(ASTNode::EXPR_STRICT_FUTURE, TOKEN_SOURCE_EXPR);
        } else {
          return new ASTNode(ASTNode::EXPR_ID, TOKEN_SOURCE_EXPR);
        }
      case TokenType::IDENTIFIER:
        return new ASTNode(ASTNode::EXPR_ID, TOKEN_SOURCE_EXPR);
      case TokenType::TK_NULL:
        return new ASTNode(ASTNode::EXPR_NULL, TOKEN_SOURCE_EXPR);
      case TokenType::TK_BOOL:
        return new ASTNode(ASTNode::EXPR_BOOL, TOKEN_SOURCE_EXPR);
      case TokenType::NUMBER:
        return new NumberLiteral(lexer.get_number_val(), TOKEN_SOURCE_EXPR);
      case TokenType::STRING:
        return new StringLiteral(std::move(lexer.get_string_val()), TOKEN_SOURCE_EXPR);
      case TokenType::LEFT_BRACK:  // [
        return parse_array_literal();
      case TokenType::LEFT_BRACE:  // {
        return parse_object_literal();
      case TokenType::LEFT_PAREN: { // (
        START_POS;
        lexer.next();   // skip (
        if (lexer.current().type == TokenType::RIGHT_PAREN) {
          return new ParenthesisExpr(nullptr, SOURCE_PARSED_EXPR);
        }
        ASTNode* value = parse_expression(false);
        if (value->is_illegal()) return value;
        if (lexer.next().type != TokenType::RIGHT_PAREN) {
          delete value;
          goto error;
        }
        return value;
      }
      case TokenType::DIV_ASSIGN:
      case TokenType::DIV: {  // /
        lexer.cursor_back(); // back to /
        if (token.type == TokenType::DIV_ASSIGN)
          lexer.cursor_back();
        u16string pattern, flag;
        token = lexer.scan_regexp_literal(pattern, flag);
        if (token.type == TokenType::REGEX) {
          return new RegExpLiteral(pattern, flag, TOKEN_SOURCE_EXPR);
        } else {
          goto error;
        }
      }
      default:
        goto error;
    }

error:
    return new ASTNode(ASTNode::ILLEGAL, TOKEN_SOURCE_EXPR);
  }

  bool parse_formal_parameter_list(std::vector<u16string_view>& params) {
    // if (!lexer.current_token().is_identifier()) {
    //   // This only happens in new Function(...)
    //   params = {};
    //   return lexer.current_token().type == TokenType::EOS;
    // }
    if (!token_match(TokenType::LEFT_PAREN)) return false;

    Token token = lexer.next();
    // NOTE(zhuzilin) the EOS is for new Function("a,b,c", "")
    while (token.type != TokenType::RIGHT_PAREN && token.type != TokenType::EOS) {
      if (token.is_identifier()) {
        params.push_back(token.text);
      }
      else if (token.type != TokenType::COMMA) {
        return false;
      }
      token = lexer.next();
    }
    if (!token_match(TokenType::RIGHT_PAREN)) return false;
    return true;
  }

  ASTNode* parse_function(bool name_required, bool func_keyword_required,
                          bool is_stmt, bool is_async) {
    START_POS;
    if (is_async) {
      assert(token_match(u"async"));
      lexer.next();
    }

    bool is_generator = false;
    if (func_keyword_required) {
      assert(token_match(u"function"));
      lexer.next();
      if (lexer.current().is(Token::MUL)) {
        is_generator = true;
        lexer.next();
      }
    } else {
      assert(lexer.current().is_identifier());
    }

    Token name = Token::none;
    std::vector<u16string_view> params;
    Function *function;
    ASTNode* body;

    if (lexer.current().is_identifier()) {
      name = lexer.current();
      if (is_stmt) {
        bool res = scope().define_symbol(VarKind::FUNCTION, name.text);
        if (!res) {
          report_error(ParsingError {
              .error_type = JS_SYNTAX_ERROR,
              .message = "Identifier '" + name.get_text_utf8() + "' has already been declared",
              .start = name.get_src_start(),
              .end = name.get_src_end()
          });
        }
      }
      lexer.next();
    }
    else if (name_required) {
      goto error;
    }

    push_scope(ScopeType::FUNC);
    // the `arguments` special object. should always be the first one.
    scope().define_symbol(VarKind::VAR, u"arguments", true);

    // allow function expression to be able to self-reference using its *name*.
    if (!is_stmt && name.is_identifier()) {
      bool res = scope().define_symbol(VarKind::FUNCTION, name.text);
      if (!res) {
        report_error(ParsingError {
            .error_type = JS_SYNTAX_ERROR,
            .message = "Identifier '" + name.get_text_utf8() + "' has already been declared",
            .start = name.get_src_start(),
            .end = name.get_src_end()
        });
      }
    }

    if (!parse_formal_parameter_list(params)) {
      goto error;
    }
    for (auto param : params) {
      scope().define_func_parameter(param);
    }

    if (lexer.next().type != TokenType::LEFT_BRACE) goto error;
    lexer.next();
    body = parse_program_or_function_body(TokenType::RIGHT_BRACE, ASTNode::FUNC_BODY);
    if (body->is_illegal()) return body;
    if (!token_match(TokenType::RIGHT_BRACE)) {
      delete body;
      goto error;
    }
    function = new Function(name, params, body, SOURCE_PARSED_EXPR);
    function->is_stmt = is_stmt;
    function->is_async = is_async;
    function->is_generator = is_generator;
    scope().register_function(function);
    return function;
error:
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_array_literal() {
    START_POS;
    assert(token_match(TokenType::LEFT_BRACK));

    auto* array = new ArrayLiteral();

    // get the token after `[`
    Token token = lexer.next();

    bool expecting_element = true;

    while (token.type != TokenType::RIGHT_BRACK) {
      if (token.type != TokenType::COMMA) {
        ASTNode *element = parse_assign_or_arrow_function(false);
        if (element->type == ASTNode::ILLEGAL) {
          delete array;
          return element;
        }
        array->add_element(element);
        expecting_element = false;
      }
      // expect an element, but get a comma
      else if (expecting_element) {
        array->add_element(nullptr);
      }
      //expect a comma and get a comma, so expect an element in the next iteration.
      else {
        expecting_element = true;
      }
      token = lexer.next();
    }

    assert(token.type == TokenType::RIGHT_BRACK);
    array->set_source(SOURCE_PARSED_EXPR);
    return array;
  }

  ASTNode* parse_object_literal() {
    using ObjectProp = ObjectLiteral::Property;

    auto token_to_prop_name = [this] (const Token& token) {
      u16string prop_name;
      if (token.is_identifier_name()) {
        prop_name = token.text;
      } else if (token.type == Token::STRING) {
        prop_name = std::move(lexer.get_string_val());
      } else if (token.type == Token::NUMBER) {
        prop_name = double_to_u16string(lexer.get_number_val());
      }
      return prop_name;
    };

    START_POS;
    assert(token_match(TokenType::LEFT_BRACE));

    auto* obj = new ObjectLiteral();
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACE) {
      if (token.is_property_name()) {
        u16string prop_name = token_to_prop_name(token);
        // Ordinary proprietary syntax: key: value
        if (lexer.peek().type == TokenType::COLON) {
          lexer.next();
          lexer.next();
          ASTNode* value = parse_assign_or_arrow_function(false);
          if (value->type == ASTNode::ILLEGAL) {
            delete obj;
            return value;
          }
          obj->set_property(ObjectProp(prop_name, value, ObjectProp::NORMAL));
        }
        // ES6+ simplified function syntax
        else if (lexer.peek().type == TokenType::LEFT_PAREN) {
          ASTNode* func = parse_function(true, false, false, false);
          if (func->is_illegal()) {
            delete obj;
            return func;
          }
          obj->set_property(ObjectProp(prop_name, func, ObjectProp::NORMAL));
        }
        // getter or setter
        else if ((token.text == u"get" || token.text == u"set") && lexer.peek().is_property_name()) {
          START_POS;
          ObjectProp::Type type = token.text == u"get" ? ObjectLiteral::Property::GETTER
                                                       : ObjectLiteral::Property::SETTER;
          prop_name = token_to_prop_name(lexer.next());

          // get prop() { ... }
          ASTNode* get_set_func = parse_function(true, false, false, false);
          if (get_set_func->is_illegal()) {
            delete obj;
            return get_set_func;
          }
          obj->set_property(ObjectProp(prop_name, get_set_func, type));
        }
        else {
          goto error;
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
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_expression(bool no_in) {
    START_POS;

    ASTNode* element = parse_assign_or_arrow_function(no_in);
    if (element->is_illegal()) return element;
    // if this is the only expression, directly return it
    if (lexer.peek().type != TokenType::COMMA) return element;

    auto* expr = new Expression();
    expr->add_element(element);
    
    while (lexer.peek().type == TokenType::COMMA) {
      lexer.next();
      lexer.next();
      element = parse_assign_or_arrow_function(no_in);
      if (element->is_illegal()) {
        delete expr;
        return element;
      }
      expr->add_element(element);
    }
    expr->set_source(SOURCE_PARSED_EXPR);
    return expr;
  }

  ASTNode* parse_assign_or_arrow_function(bool no_in) {
    START_POS;
    bool is_async_arrow_func = false;

    if (token_match(u"async")) {
      is_async_arrow_func = true;
      lexer.next();
    }

    ASTNode* lhs = parse_conditional_expression(no_in);
    if (lhs->is_illegal()) return lhs;

    Token op = lexer.peek();
    if (!op.is_assignment_operator() && !op.is(TokenType::R_ARROW)) return lhs;
    if (is_async_arrow_func && not op.is(TokenType::R_ARROW)) {
      delete lhs;
      return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
    }
    lexer.next();

    // normal assign
    if (op.is_assignment_operator()) {
      // require valid lhs expression.
      if (lhs->type != ASTNode::EXPR_LHS && lhs->type != ASTNode::EXPR_ID) {
        return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
      }
      lexer.next();
      ASTNode* rhs = parse_assign_or_arrow_function(no_in);
      if (rhs->is_illegal()) {
        delete lhs;
        return rhs;
      }
      return new AssignmentExpr(op.type, lhs, rhs, SOURCE_PARSED_EXPR);
    }
    // arrow function
    else {
      // begin gather formal parameters
      bool param_legal = check_expr_is_formal_parameter(lhs);
      if (!param_legal) {
        return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
      }

      push_scope(ScopeType::FUNC);
      std::vector<u16string_view> params;

      // no parenthesis, only one identifier
      if (lhs->is_identifier()) {
        scope().define_func_parameter(lhs->get_source());
        params.push_back(lhs->get_source());
      }
      // a list of parameters. Parentheses have been removed before.
      else if (lhs->type == ASTNode::EXPR_COMMA) {
        auto& expr_list = static_cast<Expression *>(lhs)->elements;
        for (auto expr : expr_list) {
          assert(expr->is_identifier());
          scope().define_func_parameter(expr->get_source());
          params.push_back(expr->get_source());
        }
      }
      delete lhs;
      // end gather formal parameters, begin parsing function body
      lexer.next();
      ASTNode *func_body;
      // body with brace.
      if (token_match(TokenType::LEFT_BRACE)) {
        lexer.next();
        func_body = parse_program_or_function_body(TokenType::RIGHT_BRACE, ASTNode::FUNC_BODY);
        if (func_body->is_illegal()) return func_body;
      }
      // body with only one expression
      else {
        auto *expr = parse_assign_or_arrow_function(true);
        if (expr->is_illegal()) {
          pop_scope();
          return expr;
        }
        func_body = new ProgramOrFunctionBody(ASTNode::FUNC_BODY, true);
        auto *body = static_cast<ProgramOrFunctionBody *>(func_body);
        // implicitly return
        body->add_statement(
          new ReturnStatement(expr, expr->get_source(), expr->source_start(), expr->source_end())
        );
        body->scope = pop_scope();
      }
      // finally, make an AST node for the function.
      auto *func = new Function(Token::none, std::move(params), func_body, SOURCE_PARSED_EXPR);
      func->is_arrow_func = true;
      func->is_async = is_async_arrow_func;
      scope().register_function(func);
      return func;
    }

  }

  bool check_expr_is_formal_parameter(ASTNode* param) {

    if (param->type == ASTNode::EXPR_COMMA) {
      auto& expr_list = static_cast<Expression *>(param)->elements;

      for (auto expr : expr_list) {
        if (expr->is_identifier()) continue; // legal formal parameter
        if (expr->is(ASTNode::EXPR_ASSIGN)
            && expr->as<AssignmentExpr>()->assign_type == TokenType::ASSIGN) {
          assert(false); // Not supported yet.
          continue; // legal formal parameter with default value
        }
        return false;
      }
      return true;
    }
    if (param->is_identifier()) return true;
    if (param->type == ASTNode::EXPR_PAREN) {
      return (static_cast<ParenthesisExpr *>(param)->expr == nullptr);
    }
    return false;
  }

  ASTNode* parse_conditional_expression(bool no_in) {
    START_POS;
    ASTNode* cond = parse_binary_and_unary_expression(no_in, 0);
    if (cond->is_illegal()) return cond;

    if (lexer.peek().type != TokenType::QUESTION) {
      return cond;
    }
      
    lexer.next_twice();
    ASTNode* lhs = parse_assign_or_arrow_function(no_in);
    if (lhs->is_illegal()) {
      delete cond;
      return lhs;
    }
    
    if (lexer.next().type != TokenType::COLON) {
      delete cond;
      delete lhs;
      return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
    }
    lexer.next();
    ASTNode* rhs = parse_assign_or_arrow_function(no_in);
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
    ASTNode* lhs;
    ASTNode* rhs;
    // Prefix Operators.
    Token prefix_op = lexer.current();
    // Operators that are closer to the operand are executed first.
    if (prefix_op.unary_priority() >= priority) {
      lexer.next();
      lhs = parse_binary_and_unary_expression(no_in, prefix_op.unary_priority());
      if (lhs->is_illegal()) return lhs;

      // prefix ++ and -- can only be applied to left-hand-side values.
      if (prefix_op.is(TokenType::INC) || prefix_op.is((TokenType::DEC))) {
        if (!(lhs->type == ASTNode::EXPR_ID || lhs->type == ASTNode::EXPR_LHS)) {
          return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
        }
      }
      lhs = new UnaryExpr(lhs, prefix_op, true);
      lhs->set_source(SOURCE_PARSED_EXPR);
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

        if (lhs->type != ASTNode::EXPR_BINARY && lhs->type != ASTNode::EXPR_UNARY) {
          lhs = new UnaryExpr(lhs, postfix_op, false);
          lhs->set_source(SOURCE_PARSED_EXPR);
        }
        else {
          delete lhs;
          return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
        }
      }
    }

    Token binary_op = lexer.peek();

    while (binary_op.binary_priority(no_in) > priority) {
      lexer.next_twice();
      rhs = parse_binary_and_unary_expression(no_in, binary_op.binary_priority(no_in));
      if (rhs->is_illegal()) {
        delete lhs;
        return rhs;
      }
      lhs = new BinaryExpr(lhs, rhs, binary_op, SOURCE_PARSED_EXPR);
      
      binary_op = lexer.peek();
    }

    return lhs;
  }

  ASTNode* parse_left_hand_side_expression(bool in_new_expr_ctx = false) {
    START_POS;
    Token token = lexer.current();
    ASTNode* base = nullptr;

    // need test
    if (token.text == u"new") {    
      lexer.next();
      base = parse_left_hand_side_expression(true);
      base = new NewExpr(base, SOURCE_PARSED_EXPR);
    }

    if (base == nullptr) {
      if (token.text == u"function") {
        base = parse_function(false, true, false, false);
      } else if (token.text == u"async") {
        base = parse_function(false, true, false, true);
      } else {
        base = parse_primary_expression();
      }
    }

    if (base->is_illegal()) {
      return base;
    }
    auto* lhs = new LeftHandSideExpr(base);

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
          assert(ast->type == ASTNode::EXPR_ARGS);
          auto* args = static_cast<Arguments *>(ast);
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
          if (lhs->postfixs.empty()) {
            lhs->base = nullptr;
            delete lhs;
            return base;
          }
          lhs->set_source(SOURCE_PARSED_EXPR);
          return lhs;
      }
    }
error:
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_arguments() {
    START_POS;
    assert(token_match(TokenType::LEFT_PAREN));
    std::vector<ASTNode *> arguments;

    lexer.next();
    while (!token_match(TokenType::RIGHT_PAREN)) {
      if (!token_match(TokenType::COMMA)) {
        ASTNode* argument = parse_assign_or_arrow_function(false);
        if (argument->is_illegal()) {
          for (auto arg : arguments) delete arg;
          return argument;
        }
        arguments.push_back(argument);
      }

      lexer.next();
    }

    assert(token_match(TokenType::RIGHT_PAREN));
    auto* arg_ast = new Arguments(std::move(arguments));
    arg_ast->set_source(SOURCE_PARSED_EXPR);
    return arg_ast;
  }

  ASTNode* parse_program() {
    lexer.next();
    push_scope(ScopeType::GLOBAL);
    return parse_program_or_function_body(TokenType::EOS, ASTNode::PROGRAM);
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
    auto* prog = new ProgramOrFunctionBody(syntax_type, strict);

    while (!token_match(ending_token_type)) {
      ASTNode* stmt = parse_statement();
      if (stmt->is_illegal()) {
        delete prog;
        return stmt;
      }
      prog->add_statement(stmt);
      lexer.next();
    }
    assert(token_match(ending_token_type));

    prog->scope = pop_scope();
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
        return new ASTNode(ASTNode::STMT_EMPTY, TOKEN_SOURCE_EXPR);
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
        else if (token.text == u"function") return parse_function(true, true, true, false);
        else if (token.text == u"async") return parse_function(true, true, true, true);
        else if (token.text == u"debugger") {
          if (!lexer.try_skip_semicolon()) {
            goto error;
          }
          return new ASTNode(ASTNode::STMT_DEBUG, SOURCE_PARSED_EXPR);
        }
        break;
      }
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
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_block_statement() {
    START_POS;
    assert(token_match(TokenType::LEFT_BRACE));
    auto* block = new Block();
    lexer.next();

    push_scope(ScopeType::BLOCK);

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

    block->scope = pop_scope();
    return block;
  }

  ASTNode* parse_variable_declaration(bool no_in, VarKind kind, bool is_for_init) {
    START_POS;
    Token id = lexer.current();
    assert(id.is_identifier());

    if (kind == VarKind::VAR) {
      bool res = scope().define_symbol(kind, id.text);
      if (!res) {
        report_error(ParsingError {
            .error_type = JS_SYNTAX_ERROR,
            .message = "Identifier '" + id.get_text_utf8() + "' has already been declared",
            .start = id.get_src_start(),
            .end = id.get_src_end()
        });
      }
    }

    if (lexer.peek().type != TokenType::ASSIGN) {
      if (kind == VarKind::CONST) {
        if (is_for_init) [[unlikely]] {
          auto& token = lexer.peek();
          bool legal = token.text == u"in" || token.text == u"of";
          if (not legal) {
            return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
          }
        } else {
          lexer.next();
          return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
        }
      }
      return new VarDecl(id, SOURCE_PARSED_EXPR);;
    }
    
    ASTNode* init = parse_assign_or_arrow_function(no_in);
    if (init->is_illegal()) return init;

    return new VarDecl(id, init, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_variable_statement(bool no_in) {
    START_POS;
    auto var_kind_text = lexer.current().text;
    VarKind var_kind = get_var_kind_from_str(var_kind_text);
    
    auto* var_stmt = new VarStatement(var_kind);
    ASTNode* decl;
    if (!lexer.next().is_identifier()) {
      goto error;
    }

    while (true) {
      if (lexer.current().text != u",") {
        decl = parse_variable_declaration(no_in, var_kind, false);
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
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_expression_statement() {
    START_POS;
    const Token& token = lexer.current();
    if (token.text == u"function") {
      return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
    }
    // already handled LEFT_BRACE case in caller.
    assert(token.type != TokenType::LEFT_BRACE);
    ASTNode* exp = parse_expression(false);
    if (exp->is_illegal()) return exp;

    if (!lexer.try_skip_semicolon()) {
      delete exp;
      return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
    }
    return exp;
  }

  ASTNode* parse_if_statement() {
    START_POS;
    ASTNode* cond;
    ASTNode* then_block;

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
    then_block = parse_statement();
    if (then_block->is_illegal()) {
      delete cond;
      return then_block;
    }
    
    if (lexer.peek().text == u"else") {
      lexer.next();
      lexer.next();
      ASTNode* else_block = parse_statement();
      if (else_block->is_illegal()) {
        delete cond;
        delete then_block;
        return else_block;
      }
      return new IfStatement(cond, then_block, else_block, SOURCE_PARSED_EXPR);
    }

    return new IfStatement(cond, then_block, SOURCE_PARSED_EXPR);
    
error:
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
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
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_while_statement() {
    return parse_while_or_with_statement(ASTNode::STMT_WHILE);
  }

  ASTNode* parse_with_statement() {
    return parse_while_or_with_statement(ASTNode::STMT_WITH);
  }

  ASTNode* parse_while_or_with_statement(ASTNode::Type type) {
    START_POS;
    u16string keyword = type == ASTNode::STMT_WHILE ? u"while" : u"with";
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
    if (type == ASTNode::STMT_WHILE) {
      return new WhileStatement(expr, stmt, SOURCE_PARSED_EXPR);
    }
    return new WithStatement(expr, stmt, SOURCE_PARSED_EXPR);
error:
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_for_statement() {
    START_POS;
    assert(token_match(u"for"));

    if (lexer.next().type != TokenType::LEFT_PAREN) goto error;
    lexer.next();

    ASTNode* init_expr;
    if (lexer.current().is_semicolon()) {
      return parse_for_statement(nullptr, start);  // for (; expr; expr)
    }
    else if (token_match(u"var") || token_match(u"let") || token_match(u"const")) {
      VarKind var_kind = get_var_kind_from_str(lexer.current().text);
      lexer.next();  // skip var/let/const
      if (!lexer.current().is_identifier()) goto error;

      auto var_stmt = new VarStatement(var_kind);
      Defer _defer = [&] { delete var_stmt; };

      init_expr = parse_variable_declaration(true, var_kind, true);
      if (init_expr->is_illegal()) return init_expr;
      var_stmt->add_decl(init_expr);

      // expect `in`, `of` or `,`
      // the `in` and `of` cases, that is, var VariableDeclarationNoIn in
      lexer.next();
      if (lexer.current().text == u"in" || lexer.current().text == u"of") {
        _defer.dismiss();
        return parse_for_in_statement(var_stmt, start);
      }

      // the `,` case
      while (!lexer.current().is_semicolon()) {
        if (!token_match(TokenType::COMMA) || !lexer.next().is_identifier()) {
          goto error;
        }
        init_expr = parse_variable_declaration(true, var_kind, false);
        
        if (init_expr->is_illegal()) return init_expr;
        var_stmt->add_decl(init_expr);
        lexer.next();
      }
      // for (var VariableDeclarationListNoIn; ...)
      _defer.dismiss();
      return parse_for_statement(var_stmt, start);
    }
    else {
      init_expr = parse_expression(true);
      if (init_expr->is_illegal()) {
        return init_expr;
      }
      lexer.next();
      if (lexer.current().is_semicolon()) {
        return parse_for_statement(init_expr, start);  // for ( ExpressionNoIn;
      }
      // for ( LeftHandSideExpression in
      else if (token_match(u"in") && (init_expr->is_lhs_expr() || init_expr->is_identifier())) {
        return parse_for_in_statement(init_expr, start);
      }
      else {
        delete init_expr;
        goto error;
      }
    }
error:
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_for_statement(ASTNode* init_expr, SourceLoc start) {
    assert(lexer.current().is_semicolon());

    Defer _defer = [&] { delete init_expr; };

    ASTNode* expr1 = nullptr;
    ASTNode* expr2 = nullptr;
    ASTNode* stmt = nullptr;

    if (!lexer.peek().is_semicolon()) {
      lexer.next();
      expr1 = parse_expression(false);  // for (xxx; Expression
      if (expr1->is_illegal()) {
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
      delete expr1;
      delete expr2;
      return stmt;
    }
    _defer.dismiss();
    return new ForStatement(init_expr, expr1, expr2, stmt, SOURCE_PARSED_EXPR);
error:
    delete expr1;
    delete expr2;
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_for_in_statement(ASTNode* expr0, SourceLoc start) {
    assert(token_match(u"in") || token_match(u"of"));
    auto type = lexer.current().text == u"in" ? ForInStatement::FOR_IN
                                              : ForInStatement::FOR_OF;
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
    return new ForInStatement(type, expr0, expr1, stmt, SOURCE_PARSED_EXPR);
error:
    delete expr0;
    delete expr1;
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_continue_statement() {
    return parse_continue_or_break_statement(ASTNode::STMT_CONTINUE);
  }

  ASTNode* parse_break_statement() {
    return parse_continue_or_break_statement(ASTNode::STMT_BREAK);
  }

  ASTNode* parse_continue_or_break_statement(ASTNode::Type type) {
    START_POS;
    u16string_view keyword = type == ASTNode::STMT_CONTINUE ? u"continue" : u"break";
    assert(token_match(keyword));
    if (!lexer.try_skip_semicolon()) {
      Token id = lexer.next();
      if (!id.is_identifier()) {
        return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
      }
      if (!lexer.try_skip_semicolon()) {
        return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
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
        return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
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
        return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
      }
    }
    return new ThrowStatement(expr, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_switch_statement() {
    START_POS;
    auto* switch_stmt = new SwitchStatement();
    push_scope(ScopeType::BLOCK);
    ASTNode* expr;

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
    switch_stmt->condition_expr = expr;
    if (lexer.next().type != TokenType::LEFT_BRACE) { // skip {
      goto error;
    }
    // Loop for parsing CaseClause
    lexer.next();
    while (!token_match(TokenType::RIGHT_BRACE)) {
      ASTNode* case_expr = nullptr;
      std::vector<ASTNode *> stmts;
      u16string_view type = lexer.current().text;
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
        if (switch_stmt->has_default) goto error;
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
          for (auto s : stmts) delete s;
          delete switch_stmt;
          return stmt;
        }
        stmts.push_back(stmt);
        switch_stmt->add_statement(stmt);
        lexer.next();
      }
      if (type == u"case") [[likely]] {
        switch_stmt->cases.emplace_back(case_expr, stmts);
      } else {
        switch_stmt->default_stmts = std::move(stmts);
        switch_stmt->has_default = true;
        switch_stmt->default_index = switch_stmt->cases.size();
      }
    }

    assert(token_match(TokenType::RIGHT_BRACE));
    switch_stmt->set_source(SOURCE_PARSED_EXPR);
    switch_stmt->scope = pop_scope();
    return switch_stmt;
error:
    delete switch_stmt;
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
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
      // optional catch id
      if (lexer.next().type == TokenType::LEFT_PAREN) { // skip (
        catch_id = lexer.next();
        if (!catch_id.is_identifier()) {
          goto error;
        }
        if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
          goto error;
        }
        lexer.next();
      }
      // expect {
      if (!token_match(TokenType::LEFT_BRACE)) {
        goto error;
      }
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
        delete catch_block;
        return finally_block;
      }
    }

    if (catch_block == nullptr && finally_block == nullptr) {
      goto error;
    }
    else if (finally_block == nullptr) {
      assert(catch_block);
      return new TryStatement(try_block, catch_id, catch_block, SOURCE_PARSED_EXPR);
    }
    else if (catch_block == nullptr) {
      assert(finally_block);
      return new TryStatement(try_block, finally_block, SOURCE_PARSED_EXPR);
    }
    assert(catch_block);
    assert(finally_block);
    return new TryStatement(try_block, catch_id, catch_block, finally_block, SOURCE_PARSED_EXPR);
  error:
    delete try_block;
    delete catch_block;
    delete finally_block;
    return new ASTNode(ASTNode::ILLEGAL, SOURCE_PARSED_EXPR);
  }

  ASTNode* parse_labelled_statement() {
    START_POS;
    Token id = lexer.current();  // skip identifier
    lexer.next();
    assert(token_match(TokenType::COLON));  // skip colon
    lexer.next();

    auto& label_stack = scope().get_outer_func()->label_stack;
    for (auto& rit : std::ranges::reverse_view(label_stack)) {
      if (rit == id.text) {
        report_error(ParsingError {
            .error_type = JS_SYNTAX_ERROR,
            .message = "Label '" + id.get_text_utf8() + "' has already been declared",
            .start = start,
            .end = lexer.current_src_pos()
        });
        break;
      }
    }

    label_stack.push_back(id.text);
    ASTNode* stmt = parse_statement();
    label_stack.pop_back();

    stmt->label = id.text;
    // `stmt` maybe illegal.
    return stmt;
  }

  const SmallVector<ParsingError, 10>& get_errors() { return errors; }

 private:

  bool token_match(u16string_view token) {
    return lexer.current().text == token;
  }

  bool token_match(TokenType type) {
    return lexer.current().type == type;
  }

  Scope& scope() { return *scope_chain.back(); }

  void push_scope(ScopeType scope_type) {
    Scope *parent = unlikely(scope_chain.empty()) ? nullptr : scope_chain.back().get();
    scope_chain.emplace_back(std::make_unique<Scope>(scope_type, parent));

#ifdef DBG_SCOPE
    std::cout << ">>>> push scope: " << scope().get_type_name() << "\n\n";
#endif
  }

  unique_ptr<Scope> pop_scope() {

    unique_ptr<Scope> scope = std::move(scope_chain.back());
    scope_chain.pop_back();
#ifdef DBG_SCOPE
    std::cout << "<<<< pop scope: " << scope->get_type_name() << '\n';
    std::cout << "  params count: " << scope->get_param_count()
              << ", local variables count (accumulated): " << scope->get_var_count() << '\n';
    std::cout << "  local variables in this scope:\n";

    vector<SymbolRecord *> sym_records(scope->get_symbol_table().size() - scope->get_param_count());

    for (auto& entry : scope->get_symbol_table()) {
      if (entry.second.var_kind != VarKind::DECL_FUNC_PARAM) {
        sym_records[entry.second.index] = &entry.second;
      }
    }
    for (auto rec : sym_records) {
      printf("   index:%3d  %18s  %s\n", rec->index, get_var_kind_str(rec->var_kind).c_str(),
             to_u8string(rec->name).c_str());
    }
    std::cout << '\n';
#endif
    if (scope->get_outer_func()) {
      scope->get_outer_func()->update_var_count(scope->get_var_next_index());
    }
    return scope;
  }

  void report_error(ParsingError err) {
    err.describe();
    errors.push_back(std::move(err));
  }

  u16string source;
  Lexer lexer;
  SmallVector<ParsingError, 10> errors;

  SmallVector<unique_ptr<Scope>, 10> scope_chain;

};

}  // namespace njs

#endif  // NJS_PARSER_H