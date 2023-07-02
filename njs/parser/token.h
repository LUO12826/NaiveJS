#ifndef NJS_TOKEN_H
#define NJS_TOKEN_H

#include <iostream>
#include <sstream>
#include <array>
#include <string>
#include <string_view>
#include <unordered_set>

#include "njs/include/robin_hood.h"
#include "njs/utils/helper.h"
#include "njs/parser/enum_strings.h"
#include "njs/utils/common_types.h"

namespace njs {


using robin_hood::unordered_set;
class Token {
 public:
  // Order by section 7.5, 7.6, 7.7, 7.8
  enum TokenType {
    // Identifier
    IDENTIFIER = 0,

    // Keywords
    KEYWORD,

    KW_VAR,
    KW_LET,
    KW_CONST,
    KW_FUNCTION,
    KW_RETURN,
    KW_WHILE,
    KW_FOR,
    KW_IF,
    KW_ELSE,
    KW_CASE,

    // Future Reserved Words
    FUTURE_KW,
    STRICT_FUTURE_KW,

    // Punctuator
    LEFT_BRACE,  // {
    RIGHT_BRACE,  // }
    LEFT_PAREN,  // (
    RIGHT_PAREN,  // )
    LEFT_BRACK,  // [
    RIGHT_BRACK,  // ]

    DOT,         // .
    ELLIPSIS,      // ...
    SEMICOLON,   // ;
    COMMA,       // ,
    QUESTION,    // ?
    COLON,       // :

    LT,   // <
    GT,   // >
    LE,   // <=
    GE,   // >=
    EQ,   // ==
    NE,   // !=
    EQ3,  // ===
    NE3,  // !==

    INC,  // ++
    DEC,  // --

    ADD,  // +
    SUB,  // -
    MUL,  // *
    DIV,  // /
    MOD,  // %

    LSH,   // <<
    RSH,   // >>
    UNSIGNED_RSH,  // >>>, unsigned right shift
    BIT_AND,  // &
    BIT_OR,   // |
    BIT_XOR,  // ^

    BIT_NOT,  // ~

    LOGICAL_AND,  // &&
    LOGICAL_OR,   // ||
    LOGICAL_NOT,  // !

    ASSIGN,      // =
    // The compound assign order should be the same as their 
    // calculate op.
    ADD_ASSIGN,  // +=
    SUB_ASSIGN,  // -=
    MUL_ASSIGN,  // *=
    DIV_ASSIGN,  // /=
    MOD_ASSIGN,  // %=

    LSH_ASSIGN,   // <<=
    RSH_ASSIGN,   // >>=
    UNSIGNED_RSH_ASSIGN,  // >>>=
    AND_ASSIGN,   // &=
    OR_ASSIGN,    // |=
    XOR_ASSIGN,   // ^=

    R_ARROW,      // =>

    // Null Literal
    TK_NULL,  // null

    // Bool Literal
    TK_BOOL,   // true & false

    // Number Literal
    NUMBER,

    // String Literal
    STRING,
    TEMPLATE_STR,

    // Regular Expression Literal
    REGEX,

    // Line Terminator
    LINE_TERM,

    EOS,
    NONE,
    ILLEGAL,
  };

  static const Token none;

  Token(TokenType type, std::u16string_view text, u32 start, u32 end, u32 line) :
    type(type), text(text), start(start), end(end), line(line) {}

  void set(TokenType type, std::u16string_view text, u32 start, u32 end, u32 line) {
    this->type = type;
    this->text = text;
    this->start = start;
    this->end = end;
    this->line = line;
  }

  std::string to_string() const {
    std::ostringstream oss;
    oss << "token type: " << token_type_names[(int)type] << ", text: "
        << to_utf8_string(text) << ", start: " << start << ", end: " << end;
    return oss.str();
  }

  std::string get_type_string() const {
    return {token_type_names[(int)type]};
  }

  std::string get_text_utf8() const {
    return to_utf8_string(text);
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
                  start, end - 1, line);
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

const unordered_set<std::u16string> keyword_set = {
  u"break",     u"do",       u"in",          u"typeof",
  u"case",      u"else",     u"instanceof",  u"var",
  u"catch",     u"export",   u"new",         u"void",
  u"class",     u"extends",  u"return",      u"while",
  u"const",     u"finally",  u"super",       u"with",
  u"continue",  u"for",      u"switch",      u"yield",
  u"debugger",  u"function", u"this",        u"let",
  u"default",   u"if",       u"throw",
  u"delete",    u"import",   u"try"
};

const unordered_set<std::u16string> future_reserved_word_set = {
  u"enum", u"await"
};

const unordered_set<std::u16string> strictmode_future_reserved_word_set = {
  u"implements",   u"package",   u"protected",
  u"interface",    u"private",   u"public"
};

}  // namespace njs

#endif  // NJS_TOKEN_H