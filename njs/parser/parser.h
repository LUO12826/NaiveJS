#ifndef NJS_PARSER_H
#define NJS_PARSER_H

#include <vector>

#include "njs/parser/lexer.h"
#include "njs/parser/ast.h"
#include "njs/utils/helper.h"

#define START_POS u32 start = lexer.current().start, line_start = lexer.current().line
#define SOURCE_PARSED_EXPR std::u16string_view(source.data() + start, lexer.current_pos() - start), \
                            start, lexer.current_pos(), line_start

#define TOKEN_SOURCE_EXPR token.text, token.start, token.end, token.line

namespace njs {

using std::make_unique;
using std::vector;

class Parser {
 public:
  Parser(std::u16string source) : source(source), lexer(source) {
    // For test
    var_decl_stack.push_back({});
  }

  uni_ptr<ASTNode> parse_primary_expression() {
    Token token = lexer.current();
    switch (token.type) {
      case TokenType::KEYWORD:
        if (token.text == u"this") {
          return make_unique<ASTNode>(ASTNode::AST_EXPR_THIS, TOKEN_SOURCE_EXPR);
        }
        goto error;
      case TokenType::STRICT_FUTURE_KW:
        return make_unique<ASTNode>(ASTNode::AST_EXPR_STRICT_FUTURE, TOKEN_SOURCE_EXPR);
      case TokenType::IDENTIFIER:
        return make_unique<ASTNode>(ASTNode::AST_EXPR_IDENT, TOKEN_SOURCE_EXPR);
      case TokenType::TK_NULL:
        return make_unique<ASTNode>(ASTNode::AST_EXPR_NULL, TOKEN_SOURCE_EXPR);
      case TokenType::TK_BOOL:
        return make_unique<ASTNode>(ASTNode::AST_EXPR_BOOL, TOKEN_SOURCE_EXPR);
      case TokenType::NUMBER:
        return make_unique<ASTNode>(ASTNode::AST_EXPR_NUMBER, TOKEN_SOURCE_EXPR);
      case TokenType::STRING:
        return make_unique<ASTNode>(ASTNode::AST_EXPR_STRING, TOKEN_SOURCE_EXPR);
      case TokenType::LEFT_BRACK:  // [
        return parse_array_literal();
      case TokenType::LEFT_BRACE:  // {
        return parse_object_literal();
      case TokenType::LEFT_PAREN: { // (
        lexer.next();   // skip (
        uni_ptr<ASTNode> value = parse_expression(false);
        if (value->is_illegal()) return value;
        if (lexer.next().type != TokenType::RIGHT_PAREN) {
          goto error;
        }
        return  make_unique<ParenthesisExpr>(std::move(value), value->source(),value->start_pos(),
                                              value->end_pos(), value->get_line_start());
      }
      case TokenType::DIV_ASSIGN:
      case TokenType::DIV: {  // /
        lexer.cursor_back(); // back to /
        if (token.type == TokenType::DIV_ASSIGN)
          lexer.cursor_back();
        std::u16string pattern, flag;
        token = lexer.scan_regexp_literal(pattern, flag);
        if (token.type == TokenType::REGEX) {
          return make_unique<RegExpLiteral>(pattern, flag, TOKEN_SOURCE_EXPR);
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
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, TOKEN_SOURCE_EXPR);
  }

  bool parse_formal_parameter_list(std::vector<std::u16string>& params) {
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

  uni_ptr<ASTNode> parse_function(bool name_required, bool func_keyword_required = true) {
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
    uni_ptr<ASTNode> body;

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
      if (!parse_formal_parameter_list(params)) {
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
    body = parse_function_body();

    if (body->is_illegal()) return body;

    if (lexer.current().type != TokenType::RIGHT_BRACE) {
      goto error;
    }

    return make_unique<Function>(name, std::move(params), std::move(body), SOURCE_PARSED_EXPR);
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_array_literal() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_BRACK);

    auto array = make_unique<ArrayLiteral>();

    // get the token after `[`
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACK) {
      if (token.type != TokenType::COMMA) {
        uni_ptr<ASTNode> element = parse_assignment_expression(false);
        if (element->get_type() == ASTNode::AST_ILLEGAL) {
          return element;
        }
        array->add_element((std::move(element)));
      }
      token = lexer.next();
    }

    assert(token.type == TokenType::RIGHT_BRACK);
    array->set_source(SOURCE_PARSED_EXPR);
    return array;
  }

  uni_ptr<ASTNode> parse_object_literal() {
    using ObjectProp = ObjectLiteral::Property;

    START_POS;
    assert(lexer.current().type == TokenType::LEFT_BRACE);

    uni_ptr<ObjectLiteral> obj = make_unique<ObjectLiteral>();
    Token token = lexer.next();
    while (token.type != TokenType::RIGHT_BRACE) {
      if (token.is_property_name()) {
        // getter or setter
        if ((token.text == u"get" || token.text == u"set") && lexer.peek().is_property_name()) {
          START_POS;

          ObjectProp::Type type = token.text == u"get" ? ObjectLiteral::Property::GETTER
                                                       : ObjectLiteral::Property::SETTER;

          Token key = lexer.next();  // skip property name
          if (!key.is_property_name()) goto error;

          uni_ptr<ASTNode> get_set_func = parse_function(true, false);
          if (get_set_func->is_illegal()) {
            goto error;
          }

          obj->set_property(ObjectProp(key, std::move(get_set_func), type));
        }
        else {
          if (lexer.next().type != TokenType::COLON) goto error;
          
          lexer.next();
          uni_ptr<ASTNode> value = parse_assignment_expression(false);
          if (value->get_type() == ASTNode::AST_ILLEGAL) goto error;
          obj->set_property(ObjectProp(token, std::move(value), ObjectProp::NORMAL));
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
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_expression(bool no_in) {
    START_POS;

    uni_ptr<ASTNode> element = parse_assignment_expression(no_in);
    if (element->is_illegal()) {
      return element;
    }
    // NOTE(zhuzilin) If expr has only one element, then just return the element.
    // lexer.checkpoint();
    if (lexer.peek().type != TokenType::COMMA) {
      return element;
    }

    auto expr = make_unique<Expression>();
    expr->add_element(std::move(element));
    
    while (lexer.peek().type == TokenType::COMMA) {
      lexer.next();
      lexer.next();
      element = parse_assignment_expression(no_in);
      if (element->is_illegal()) {
        return element;
      }
      expr->add_element(std::move(element));
    }
    expr->set_source(SOURCE_PARSED_EXPR);
    return expr;
  }

  uni_ptr<ASTNode> parse_assignment_expression(bool no_in) {
    START_POS;

    uni_ptr<ASTNode> lhs = parse_conditional_expression(no_in);
    if (lhs->is_illegal()) return lhs;

    // Not LeftHandSideExpression
    if (lhs->get_type() != ASTNode::AST_EXPR_LHS) return lhs;
    
    Token op = lexer.peek();
    if (!op.is_assignment_operator()) return lhs;
    
    lexer.next();
    lexer.next();
    uni_ptr<ASTNode> rhs = parse_assignment_expression(no_in);
    if (rhs->is_illegal()) {
      return rhs;
    }

    return make_unique<BinaryExpr>(std::move(lhs), std::move(rhs), std::move(op), SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_conditional_expression(bool no_in) {
    START_POS;
    uni_ptr<ASTNode> cond = parse_binary_and_unary_expression(no_in, 0);
    if (cond->is_illegal()) return cond;

    if (lexer.peek().type != TokenType::QUESTION) return cond;
      
    lexer.next();
    lexer.next();
    uni_ptr<ASTNode> lhs = parse_assignment_expression(no_in);

    if (lhs->is_illegal()) return lhs;
    
    if (lexer.next().type != TokenType::COLON) {
      return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }

    lexer.next();
    uni_ptr<ASTNode> rhs = parse_assignment_expression(no_in);
    if (lhs->is_illegal()) return rhs;

    auto triple = make_unique<TernaryExpr>(std::move(cond), std::move(lhs), std::move(rhs));
    triple->set_source(SOURCE_PARSED_EXPR);
    return triple;
  }

  uni_ptr<ASTNode> parse_binary_and_unary_expression(bool no_in, int priority) {
    START_POS;
    uni_ptr<ASTNode> lhs;
    uni_ptr<ASTNode> rhs;
    // Prefix Operators.
    Token prefix_op = lexer.current();
    // NOTE(zhuzilin) !!a = !(!a)
    if (prefix_op.unary_priority() >= priority) {
      lexer.next();
      lhs = parse_binary_and_unary_expression(no_in, prefix_op.unary_priority());
      if (lhs->is_illegal()) return lhs;

      lhs = make_unique<UnaryExpr>(std::move(lhs), prefix_op, true);
    }
    else {
      lhs = parse_left_hand_side_expression();
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
          lhs = make_unique<UnaryExpr>(std::move(lhs), postfix_op, false);
          lhs->set_source(SOURCE_PARSED_EXPR);
        }
        else {
          return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
        }
      }
    }

    Token binary_op = lexer.peek();

    while (binary_op.binary_priority(no_in) > priority) {
      lexer.next();
      lexer.next();
      rhs = parse_binary_and_unary_expression(no_in, binary_op.binary_priority(no_in));
      if (rhs->is_illegal()) return rhs;
      lhs = make_unique<BinaryExpr>(std::move(lhs), std::move(rhs), binary_op, SOURCE_PARSED_EXPR);
      
      binary_op = lexer.peek();
    }
    
    lhs->set_source(SOURCE_PARSED_EXPR);
    return lhs;
  }

  uni_ptr<ASTNode> parse_left_hand_side_expression() {
    START_POS;
    Token token = lexer.current();
    uni_ptr<ASTNode> base;
    u32 new_count = 0;

    // need test
    while (token.text == u"new") {
      new_count++;
      token = lexer.next();
    }

    if (token.text == u"function") {
      base = parse_function(false);
    }
    else {
      base = parse_primary_expression();
    }
    if (base->is_illegal()) {
      return base;
    }
    auto lhs = make_unique<LeftHandSideExpr>(std::move(base), new_count);

    while (true) {
      
      token = lexer.peek();
      switch (token.type) {
        case TokenType::LEFT_PAREN: {  // (
          lexer.next();
          uni_ptr<ASTNode> ast = parse_arguments();
          if (ast->is_illegal()) {
            return ast;
          }

          assert(ast->get_type() == ASTNode::AST_EXPR_ARGS);
          lhs->AddArguments(static_ptr_cast<Arguments>(std::move(ast)));
          break;
        }
        case TokenType::LEFT_BRACK: {  // [
          lexer.next();  // skip [
          lexer.next();
          uni_ptr<ASTNode> index = parse_expression(false);
          if (index->is_illegal()) {
            return index;
          }
          token = lexer.next();  // skip ]
          if (token.type != TokenType::RIGHT_BRACK) {
            goto error;
          }
          lhs->AddIndex(std::move(index));
          break;
        }
        case TokenType::DOT: {  // .
          lexer.next();
          token = lexer.next();  // read identifier name
          if (!token.is_identifier_name()) {
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
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_arguments() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_PAREN);
    std::vector<uni_ptr<ASTNode>> args;
    uni_ptr<ASTNode> arg;

    lexer.next();
    while (lexer.current().type != TokenType::RIGHT_PAREN) {
      if (lexer.current().type != TokenType::COMMA) {
        arg = parse_assignment_expression(false);

        if (arg->is_illegal()) return arg;
        args.emplace_back(std::move(arg));
      }

      lexer.next();
    }

    assert(lexer.current().type == TokenType::RIGHT_PAREN);
    auto arg_ast = make_unique<Arguments>(std::move(args));
    arg_ast->set_source(SOURCE_PARSED_EXPR);
    return arg_ast;
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_function_body() {
    return parse_program_or_function_body(TokenType::RIGHT_BRACE, ASTNode::AST_FUNC_BODY);
  }

  uni_ptr<ASTNode> parse_program() {
    lexer.next();
    return parse_program_or_function_body(TokenType::EOS, ASTNode::AST_PROGRAM);
  }

  uni_ptr<ASTNode> parse_program_or_function_body(TokenType ending_token_type, ASTNode::Type syntax_type) {
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

    auto prog = make_unique<ProgramOrFunctionBody>(syntax_type, strict);
    var_decl_stack.push_back({});

    uni_ptr<ASTNode> element;
    
    while (lexer.current().type != ending_token_type) {
      if (lexer.current().text == u"function") {
        element = parse_function(true);
        if (element->is_illegal()) {
          return element;
        }
        prog->add_function_decl(std::move(element));
      }
      else {
        element = parse_statement();
        if (element->is_illegal()) {
          return element;
        }
        prog->add_statement(std::move(element));
      }
      lexer.next();
    }
    assert(lexer.current().type == ending_token_type);
    prog->set_var_decls(std::move(var_decl_stack.back()));
    var_decl_stack.pop_back();
    prog->set_source(SOURCE_PARSED_EXPR);
    return prog;
  }

  uni_ptr<ASTNode> parse_statement() {
    START_POS;
    const Token& token = lexer.current();

    switch (token.type) {
      case TokenType::LEFT_BRACE:  // {
        return parse_block_statement();
      case TokenType::SEMICOLON:  // ;
        return make_unique<ASTNode>(ASTNode::AST_STMT_EMPTY, TOKEN_SOURCE_EXPR);
      case TokenType::KEYWORD: {
        if (token.text == u"var") return parse_variable_statement(false);
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
        else if (token.text == u"function") return parse_function(true, true);
        else if (token.text == u"debugger") {
          if (!lexer.try_skip_semicolon()) {
            goto error;
          }
          return make_unique<ASTNode>(ASTNode::AST_STMT_DEBUG, SOURCE_PARSED_EXPR);
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
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_block_statement() {
    START_POS;
    assert(lexer.current().type == TokenType::LEFT_BRACE);
    auto block = make_unique<Block>();
    lexer.next();
    while (lexer.current().type != TokenType::RIGHT_BRACE) {
      uni_ptr<ASTNode> stmt = parse_statement();
      if (stmt->is_illegal()) {
        return stmt;
      }
      block->add_statement(std::move(stmt));
      lexer.next();
    }

    assert(lexer.current().type == TokenType::RIGHT_BRACE);
    block->set_source(SOURCE_PARSED_EXPR);
    return block;
  }

  uni_ptr<ASTNode> parse_variable_declaration(bool no_in) {
    START_POS;
    Token id = lexer.current();
    
    assert(id.is_identifier());

    if (lexer.peek().type != TokenType::ASSIGN) {
      auto var_decl = make_unique<VarDecl>(id, SOURCE_PARSED_EXPR);
      var_decl_stack.back().push_back(var_decl.get());
      return var_decl;
    }
    
    uni_ptr<ASTNode> init = parse_assignment_expression(no_in);
    if (init->is_illegal()) return init;
    
    auto var_decl =  make_unique<VarDecl>(id, std::move(init), SOURCE_PARSED_EXPR);
    var_decl_stack.back().push_back(var_decl.get());
    return var_decl;
  }

  uni_ptr<ASTNode> parse_variable_statement(bool no_in) {
    START_POS;
    assert(lexer.current().text == u"var");
    auto var_stmt = make_unique<VarStatement>();
    uni_ptr<ASTNode> decl;
    if (!lexer.next().is_identifier()) {
      goto error;
    }
    // Similar to parse_expression
    decl = parse_variable_declaration(no_in);
    if (decl->is_illegal()) return decl;

    var_stmt->add_decl(std::move(decl));

    while (!lexer.try_skip_semicolon()) {
      if (lexer.next().text != u",") goto error;
      lexer.next();
      decl = parse_variable_declaration(no_in);
      if (decl->is_illegal()) return decl;

      var_stmt->add_decl(std::move(decl));
    }

    var_stmt->set_source(SOURCE_PARSED_EXPR);
    return var_stmt;
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_expression_statement() {
    START_POS;
    const Token& token = lexer.current();
    if (token.text == u"function") {
      return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    // already handled LEFT_BRACE case in caller.
    assert(token.type != TokenType::LEFT_BRACE);
    uni_ptr<ASTNode> exp = parse_expression(false);
    if (exp->is_illegal()) return exp;

    if (!lexer.try_skip_semicolon()) {
      return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
    }
    return exp;
  }

  uni_ptr<ASTNode> parse_if_statement() {
    START_POS;
    uni_ptr<ASTNode> cond;
    uni_ptr<ASTNode> if_block;
    uni_ptr<ASTNode> else_block;

    assert(lexer.current().text == u"if");
    lexer.next();
    lexer.next();
    cond = parse_expression(false);
    if (cond->is_illegal()) return cond;

    if (lexer.next().type != TokenType::RIGHT_PAREN) {
      goto error;
    }
    lexer.next();
    if_block = parse_statement();
    if (if_block->is_illegal()) {
      return if_block;
    }

    if (lexer.peek().text == u"else") {
      lexer.next();
      lexer.next();
      else_block = parse_statement();
      if (else_block->is_illegal()) {
        return else_block;
      }
    }
    return make_unique<IfStatement>(std::move(cond), std::move(if_block),
                                    std::move(else_block), SOURCE_PARSED_EXPR);
    
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_do_while_statement() {
    START_POS;
    assert(lexer.current().text == u"do");
    uni_ptr<ASTNode> cond;
    uni_ptr<ASTNode> loop_block;
    lexer.next();
    loop_block = parse_statement();
    if (loop_block->is_illegal()) {
      return loop_block;
    }
    if (lexer.next().text != u"while") {  // skip while
      goto error;
    }
    if (lexer.next().type != TokenType::LEFT_PAREN) {  // skip (
      goto error;
    }

    lexer.next();
    cond = parse_expression(false);
    if (cond->is_illegal()) {
      return cond;
    }
    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      goto error;
    }
    if (!lexer.try_skip_semicolon()) {
      goto error;
    }
    return make_unique<DoWhileStatement>(std::move(cond), std::move(loop_block), SOURCE_PARSED_EXPR);
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_while_statement() {
    return parse_while_or_with_statement(ASTNode::AST_STMT_WHILE);
  }

  uni_ptr<ASTNode> parse_with_statement() {
    return parse_while_or_with_statement(ASTNode::AST_STMT_WITH);
  }

  uni_ptr<ASTNode> parse_while_or_with_statement(ASTNode::Type type) {
    START_POS;
    std::u16string keyword = type == ASTNode::AST_STMT_WHILE ? u"while" : u"with";
    assert(lexer.current().text == keyword);
    uni_ptr<ASTNode> expr;
    uni_ptr<ASTNode> stmt;
    if (lexer.next().type != TokenType::LEFT_PAREN) { // read (
      goto error;
    }

    lexer.next();
    expr = parse_expression(false);
    if (expr->is_illegal()) return expr;

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // read )
      goto error;
    }

    lexer.next();
    stmt = parse_statement();
    if (stmt->is_illegal()) {
      return stmt;
    }
    if (type == ASTNode::AST_STMT_WHILE) {
      return make_unique<WhileStatement>(std::move(expr), std::move(stmt), SOURCE_PARSED_EXPR);
    }
    return make_unique<WithStatement>(std::move(expr), std::move(stmt), SOURCE_PARSED_EXPR);
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_for_statement() {
    START_POS;
    assert(lexer.current().text == u"for");

    uni_ptr<ASTNode> init;
    std::vector<uni_ptr<ASTNode>> init_exprs;

    if (lexer.next().type != TokenType::LEFT_PAREN) goto error;

    lexer.next();
    if (lexer.current().is_semicolon()) {
      return parse_for_statement({}, start, line_start);  // for (;
    }
    else if (lexer.current().text == u"var") {
      lexer.next();  // skip var
      // NOTE(zhuzilin) the starting token for parse_variable_declaration
      // must be identifier. This is for better error code.
      if (!lexer.current().is_identifier()) goto error;

      init = parse_variable_declaration(true);
      if (init->is_illegal()) return init;

      // expect `in`, `,`
      // the `in` case
      // var VariableDeclarationNoIn in
      if (lexer.next().text == u"in") {
        return parse_for_in_statement(std::move(init), start, line_start);
      }
      
      // the `,` case
      init_exprs.push_back(std::move(init));
      while (!lexer.current().is_semicolon()) {
        // NOTE(zhuzilin) the starting token for parse_variable_declaration
        // must be identifier. This is for better error code.
        if (lexer.current().type != TokenType::COMMA || !lexer.next().is_identifier()) {
          goto error;
        }

        init = parse_variable_declaration(true);
        if (init->is_illegal()) return init;
        
        init_exprs.push_back(std::move(init));
        lexer.next();
      } 
      // for (var VariableDeclarationListNoIn; ...)
      return parse_for_statement(std::move(init_exprs), start, line_start);
    }
    else {
      init = parse_expression(true);
      if (init->is_illegal()) {
        return init;
      }
      lexer.next();
      if (lexer.current().is_semicolon()) {
        // for ( ExpressionNoIn;
        init_exprs.push_back(std::move(init));
        return parse_for_statement(std::move(init_exprs), start, line_start);
      }

      // for ( LeftHandSideExpression in
      else if (lexer.current().text == u"in" && init->get_type() == ASTNode::AST_EXPR_LHS) {
        return parse_for_in_statement(std::move(init), start, line_start);  
      }
      else {
        goto error;
      }
    }
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode>
  parse_for_statement(std::vector<uni_ptr<ASTNode>> init_exprs, u32 start, u32 line_start) {

    assert(lexer.current().is_semicolon());
    uni_ptr<ASTNode> expr1;
    uni_ptr<ASTNode> expr2;
    uni_ptr<ASTNode> stmt;

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

      if (expr2->is_illegal()) return expr2;
    }

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // read the )
      goto error;
    }

    lexer.next();
    stmt = parse_statement();
    if (stmt->is_illegal()) return stmt;

    return make_unique<ForStatement>(std::move(init_exprs), std::move(expr1), std::move(expr2),
                                      std::move(stmt), SOURCE_PARSED_EXPR);
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_for_in_statement(uni_ptr<ASTNode> expr0, u32 start, u32 line_start) {
    assert(lexer.current().text == u"in");
    lexer.next();
    uni_ptr<ASTNode> expr1 = parse_expression(false);  // for ( xxx in Expression
    uni_ptr<ASTNode> stmt;

    if (expr1->is_illegal()) return expr1;

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      goto error;
    }

    lexer.next();
    stmt = parse_statement();
    if (stmt->is_illegal()) return stmt;

    return make_unique<ForInStatement>(std::move(expr0), std::move(expr1), std::move(stmt), SOURCE_PARSED_EXPR);
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_continue_statement() {
    return parse_continue_or_break_statement(ASTNode::AST_STMT_CONTINUE);
  }

  uni_ptr<ASTNode> parse_break_statement() {
    return parse_continue_or_break_statement(ASTNode::AST_STMT_BREAK);
  }

  uni_ptr<ASTNode> parse_continue_or_break_statement(ASTNode::Type type) {
    START_POS;
    std::u16string_view keyword = type == ASTNode::AST_STMT_CONTINUE ? u"continue" : u"break";
    assert(lexer.current().text == keyword);
    if (!lexer.try_skip_semicolon()) {
      Token id = lexer.next();
      if (!id.is_identifier()) {
        return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
      if (!lexer.try_skip_semicolon()) {
        return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
      return make_unique<ContinueOrBreak>(type, id, SOURCE_PARSED_EXPR);
    }
    return make_unique<ContinueOrBreak>(type, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_return_statement() {
    START_POS;
    assert(lexer.current().text == u"return");
    uni_ptr<ASTNode> expr;
    
    if (!lexer.try_skip_semicolon()) {
      lexer.next();
      expr = parse_expression(false);
      if (expr->is_illegal()) {
        return expr;
      }
      if (!lexer.try_skip_semicolon()) {
        return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
    }
    return make_unique<ReturnStatement>(std::move(expr), SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_throw_statement() {
    START_POS;
    assert(lexer.current().text == u"throw");
    uni_ptr<ASTNode> expr;
    if (!lexer.try_skip_semicolon()) {

      lexer.next();
      expr = parse_expression(false);
      if (expr->is_illegal()) return expr;

      if (!lexer.try_skip_semicolon()) {
        return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
      }
    }
    return make_unique<ThrowStatement>(std::move(expr), SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_switch_statement() {
    START_POS;
    auto switch_stmt = make_unique<SwitchStatement>();
    uni_ptr<ASTNode> expr;
    // Token token = lexer.current();
    assert(lexer.current().text == u"switch");
    if (lexer.next().type != TokenType::LEFT_PAREN) { // skip (
      goto error;
    }

    lexer.next();
    expr = parse_expression(false);
    if (expr->is_illegal()) return expr;

    if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
      goto error;
    }
    switch_stmt->SetExpr(std::move(expr));
    if (lexer.next().type != TokenType::LEFT_BRACE) { // skip {
      goto error;
    }
    // Loop for parsing CaseClause
    lexer.next();
    while (lexer.current().type != TokenType::RIGHT_BRACE) {
      uni_ptr<ASTNode> case_expr;
      std::vector<uni_ptr<ASTNode>> stmts;
      std::u16string_view type = lexer.current().text;

      if (type == u"case") {
        lexer.next();  // skip case
        case_expr = parse_expression(false);
        if (case_expr->is_illegal()) {
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
        goto error;
      }
      // parse StatementList
      lexer.next();
      // the statements in each case
      while (lexer.current().text != u"case" && lexer.current().text != u"default" &&
              lexer.current().type != TokenType::RIGHT_BRACE) {

        uni_ptr<ASTNode> stmt = parse_statement();
        if (stmt->is_illegal()) return stmt;

        stmts.push_back(std::move(stmt));
        lexer.next();
      }

      if (type == u"case") {
        if (switch_stmt->has_default_clause) {
          switch_stmt->AddAfterDefaultCaseClause(SwitchStatement::CaseClause(std::move(case_expr), std::move(stmts)));
        }
        else {
          switch_stmt->AddBeforeDefaultCaseClause(SwitchStatement::CaseClause(std::move(case_expr), std::move(stmts)));
        }
      }
      else {
        switch_stmt->SetDefaultClause(std::move(stmts));
      }
    }

    assert(lexer.current().type == TokenType::RIGHT_BRACE);
    switch_stmt->set_source(SOURCE_PARSED_EXPR);
    return switch_stmt;
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_try_statement() {
    START_POS;
    assert(lexer.current().text == u"try");

    Token catch_id = Token::none;
    uni_ptr<ASTNode> try_block;
    uni_ptr<ASTNode> catch_block;
    uni_ptr<ASTNode> finally_block;

    if (lexer.next().type != TokenType::LEFT_BRACE) {
      goto error;
    }
    try_block = parse_block_statement();
    if (try_block->is_illegal())
      return try_block;
    
    if (lexer.peek().text == u"catch") {
      lexer.next();
      if (lexer.next().type != TokenType::LEFT_PAREN) {  // skip (
        goto error;
      }
      catch_id = lexer.next();  // skip identifier
      if (!catch_id.is_identifier()) {
        goto error;
      }
      if (lexer.next().type != TokenType::RIGHT_PAREN) {  // skip )
        goto error;
      }

      lexer.next();
      catch_block = parse_block_statement();
      if (catch_block->is_illegal()) {
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
        return finally_block;
      }
    }

    if (catch_block == nullptr && finally_block == nullptr) {
      goto error;
    }

    if (catch_block != nullptr) {
      assert(catch_id.is_identifier());
    }

    return make_unique<TryStatement>(std::move(try_block), catch_id, std::move(catch_block),
                                      std::move(finally_block), SOURCE_PARSED_EXPR);
error:
    return make_unique<ASTNode>(ASTNode::AST_ILLEGAL, SOURCE_PARSED_EXPR);
  }

  uni_ptr<ASTNode> parse_labelled_statement() {
    START_POS;
    Token id = lexer.current();  // skip identifier
    lexer.next();
    assert(lexer.current().type == TokenType::COLON);  // skip colon
    lexer.next();
    uni_ptr<ASTNode> stmt = parse_statement();

    if (stmt->is_illegal()) return stmt;
    return make_unique<LabelledStatement>(id, std::move(stmt), SOURCE_PARSED_EXPR);
  }

 private:
  std::u16string source;
  Lexer lexer;

  vector<vector<VarDecl*>> var_decl_stack;
};

}  // namespace njs

#endif  // NJS_PARSER_H