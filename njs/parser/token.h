#ifndef NJS_TOKEN_H
#define NJS_TOKEN_H

#include <iostream>
#include <sstream>
#include <array>
#include <string>
#include <string_view>
#include <unordered_set>
#include "token_type.h"
#include "njs/utils/helper.h"
#include "njs/utils/common_types.h"

namespace njs {

class Token {
 public:

  static const Token none;

  Token(TokenType type, std::u16string_view text, u32 start, u32 end, u32 line = 0) :
    type(type), text(text), start(start), end(end), line(line) {}

  void set(TokenType type, std::u16string_view text, u32 start, u32 end, u32 line = 0) {
    this->type = type;
    this->text = text;
    this->start = start;
    this->end = end;
    this->line = line;
  }

  std::string to_string() const {
    std::ostringstream oss;
    oss << "token type: " << token_type_names[(int)type] << ", text: "
        << test::to_utf8_string(text) << ", start: " << start << ", end: " << end;
    return oss.str();
  }

  inline bool is_assignment_operator() const {
    switch(type) {
      case ASSIGN:      // =
      case ADD_ASSIGN:  // +=
      case SUB_ASSIGN:  // -=
      case MUL_ASSIGN:  // *=
      case MOD_ASSIGN:  // %=
      case DIV_ASSIGN:  // /=

      case LSH_ASSIGN:   // <<=
      case RSH_ASSIGN:   // >>=
      case UNSIGNED_RSH_ASSIGN:  // >>>=
      case AND_ASSIGN:   // &=
      case OR_ASSIGN:    // |=
      case XOR_ASSIGN:   // ^=
        return true;
      default:
        return false;
    }
  }

  inline bool is_line_terminator() const { return type == LINE_TERM; }

  inline bool is_identifier_name() const {
    return type == IDENTIFIER || type == KEYWORD ||
           type == FUTURE_KW || type == STRICT_FUTURE_KW ||
           type == TK_NULL || type == TK_BOOL;
  }

  inline bool is_property_name() const {
    return is_identifier_name() || type == STRING || type == NUMBER;
  }

  inline bool is_semicolon() const { return type == SEMICOLON; }

  inline bool is_identifier() const { return type == IDENTIFIER || type == STRICT_FUTURE_KW; }

  inline bool is_binary_logical() const { return type == LOGICAL_AND || type == LOGICAL_OR; }

  inline bool is_compound_assign() const { return ADD_ASSIGN <= type && type <= XOR_ASSIGN; }

  inline int binary_priority(bool no_in) const {
    switch (type) {
      // Binary
      case LOGICAL_OR:  // ||
        return 2;
      case LOGICAL_AND:  // &&
        return 3;
      case BIT_OR:  // |
        return 4;
      case BIT_XOR:  // ^
        return 5;
      case BIT_AND:  // &
        return 6;
      case EQ:   // ==
      case NE:   // !=
      case EQ3:  // ===
      case NE3:  // !==
        return 7;
      case LT:   // <
      case GT:   // >
      case LE:   // <=
      case GE:   // >=
        return 8;
      case LSH:   // <<
      case RSH:   // >>
      case UNSIGNED_RSH:  // >>>
        return 9;
      case ADD:
      case SUB:
        return 10;
      case MUL:
      case DIV:
      case MOD:
        return 11;

      case KEYWORD:
        if (text == u"instanceof") {
          return 8;
        // To prevent parsing for(a in b).
        } else if (!no_in && text == u"in") {
          return 8;
        }
        [[fallthrough]];
      default:
        return -1;
    }
  }

  inline int unary_priority() const {
    switch (type) {
      // Prefix
      case INC:
      case DEC:
      case ADD:
      case SUB:
      case BIT_NOT:
      case LOGICAL_NOT:
        return 100;  // UnaryExpresion always have higher priority.

      case KEYWORD:
        if (text == u"delete" || text == u"void" || text == u"typeof") {
          return 100;
        }
        [[fallthrough]];
      default:
        return -1;
    }
  }

  inline int postfix_priority() const {
    switch (type) {
      case INC:
      case DEC:
        return 200;  // UnaryExpresion always have higher priority.
      default:
        return -1;
    }
  }

  Token compound_assign_to_binary() const {
    assert(is_compound_assign());
    return Token((TokenType)(type - ADD_ASSIGN + ADD),
                  text.substr(0, text.size() - 1),
                  start,
                  end - 1);
  }

  inline const std::u16string_view& get_text_ref() const { return text; }

  TokenType type;
  std::u16string_view text;
  u32 start;
  u32 end;

  u32 line;

  SourceLocation start_loc;
  SourceLocation end_loc;
};

const Token Token::none = Token(TokenType::NONE, u"", 0, 0);

const std::unordered_set<std::u16string> keyword_set = {
  u"break",     u"do",       u"in",          u"typeof",
  u"case",      u"else",     u"instanceof",  u"var",
  u"catch",     u"export",   u"new",         u"void",
  u"class",     u"extends",  u"return",      u"while",
  u"const",     u"finally",  u"super",       u"with",
  u"continue",  u"for",      u"switch",      u"yield",
  u"debugger",  u"function", u"this",
  u"default",   u"if",       u"throw",
  u"delete",    u"import",   u"try"
};

const std::unordered_set<std::u16string> future_reserved_word_set = {
  u"enum", u"await"
};

const std::unordered_set<std::u16string> strictmode_future_reserved_word_set = {
  u"implements",   u"package",   u"protected",
  u"interface",    u"private",   u"public"
};

}  // namespace njs

#endif  // NJS_TOKEN_H