#ifndef NJS_TOKEN_H
#define NJS_TOKEN_H

#include <iostream>
#include <sstream>
#include <array>
#include <string>
#include <string_view>
#include <unordered_set>

#include "njs/common/common_types.h"
#include "njs/common/enum_strings.h"
#include "njs/include/robin_hood.h"
#include "njs/common/conversion_helper.h"

namespace njs {

using robin_hood::unordered_flat_set;
using std::u16string_view;

class Token {
 public:
  // has corresponding string representation, note to modify when adding
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

    LOGI_AND_ASSIGN,   // &&=
    LOGI_OR_ASSIGN,    // ||=

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

  Token(TokenType type, u16string_view text, u32 start, u32 end, u32 start_col, u32 line) :
    type(type), text(text), start(start), end(end), start_col(start_col), line(line) {}

  void set(TokenType type, u16string_view text, u32 start, u32 end, u32 start_col, u32 line) {
    this->type = type;
    this->text = text;
    this->start = start;
    this->end = end;
    this->start_col = start_col;
    this->line = line;
  }

  std::string to_string() const {
    std::ostringstream oss;
    oss << "token type: " << token_type_names[(int)type] << ", text: "
        << to_u8string(text) << ", start: " << start << ", end: " << end;
    return oss.str();
  }

  std::string get_type_string() const {
    return {token_type_names[(int)type]};
  }

  std::string get_text_utf8() const {
    return to_u8string(text);
  }

  bool is(TokenType type) const {
    return this->type == type;
  }

  bool is_assignment_operator() const {
    return ASSIGN <= type && type <= LOGI_OR_ASSIGN;
  }

  [[maybe_unused]] inline bool is_line_terminator() const { return type == LINE_TERM; }

  bool is_identifier_name() const {
    return type == IDENTIFIER || type == KEYWORD ||
           type == FUTURE_KW || type == STRICT_FUTURE_KW ||
           type == TK_NULL || type == TK_BOOL;
  }

  bool is_property_name() const {
    return is_identifier_name() || type == STRING || type == NUMBER;
  }

  bool is_semicolon() const { return type == SEMICOLON; }

  bool is_identifier() const { return type == IDENTIFIER || type == STRICT_FUTURE_KW; }

  bool is_binary_logical() const { return type == LOGICAL_AND || type == LOGICAL_OR; }

  bool is_compound_assign() const { return ADD_ASSIGN <= type && type <= XOR_ASSIGN; }

  int binary_priority(bool no_in) const {
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
        return -1;
      default:
        return -1;
    }
  }

  // all prefix unary operators are given the same priority. In this way,
  // operators that are closer to the operand are executed first.
  int unary_priority() const {
    switch (type) {
      // Prefix
      case INC:
      case DEC:
      case ADD:
      case SUB:
      case BIT_NOT:
      case LOGICAL_NOT:
        return 100;

      case KEYWORD:
        if (text == u"delete" || text == u"void" || text == u"typeof" || text == u"yield") {
          return 100;
        }
        return -1;
      case FUTURE_KW:
        if (text == u"await") {
          return 100;
        }
        return -1;
      default:
        return -1;
    }
  }

  int postfix_priority() const {
    switch (type) {
      case INC:
      case DEC:
        return 200;
      default:
        return -1;
    }
  }

  Token compound_assign_to_binary() const {
    assert(is_compound_assign());
    return Token((TokenType)(type - ADD_ASSIGN + ADD),
                  text.substr(0, text.size() - 1),
                  start, end - 1, start_col, line);
  }

  SourceLoc get_src_start() const {
    return {line, start_col, start};
  }

  SourceLoc get_src_end() const {
    return {line, start_col + (start - end), end};
  }

  u16string_view text;
  TokenType type;
  u32 start;
  u32 end;
  u32 start_col;
  u32 line;
};

inline const Token Token::none = Token(TokenType::NONE, u"", 0, 0, 0, 0);

const unordered_flat_set<std::u16string> keywords = {
  u"break",     u"do",       u"in",          u"typeof",
  u"case",      u"else",     u"instanceof",  u"var",
  u"catch",     u"export",   u"new",         u"void",
  u"class",     u"extends",  u"return",      u"while",
  u"const",     u"finally",  u"super",       u"with",
  u"continue",  u"for",      u"switch",      u"yield",
  u"debugger",  u"function", u"this",        u"let",
  u"default",   u"if",       u"throw",       u"async",
  u"delete",    u"import",   u"try"
};

const unordered_flat_set<std::u16string> future_reserved_words = {
  u"enum", u"await"
};

const unordered_flat_set<std::u16string> strict_future_reserved_words = {
  u"implements",   u"package",   u"protected",
  u"interface",    u"private",   u"public"
};

}  // namespace njs

#endif  // NJS_TOKEN_H