#ifndef NJS_TOKEN_TYPE_H
#define NJS_TOKEN_TYPE_H

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

const char *token_type_names[] = {
  "IDENTIFIER",
  "KEYWORD",

  "KW_VAR",
  "KW_LET",
  "KW_CONST",
  "KW_FUNCTION",
  "KW_RETURN",
  "KW_WHILE",
  "KW_FOR",
  "KW_IF",
  "KW_ELSE",
  "KW_CASE",

  "FUTURE_KW",
  "STRICT_FUTURE_KW",
  "LEFT_BRACE",
  "RIGHT_BRACE",
  "LEFT_PAREN",
  "RIGHT_PAREN",
  "LEFT_BRACK",
  "RIGHT_BRACK",

  "DOT",
  "ELLIPSIS",
  "SEMICOLON",
  "COMMA",
  "QUESTION",
  "COLON",

  "LT",
  "GT",
  "LE",
  "GE",
  "EQ",
  "NE",
  "EQ3",
  "NE3",
  "INC",
  "DEC",
  "ADD",
  "SUB",
  "MUL",
  "DIV",
  "MOD",

  "LSH",
  "RSH",
  "UNSIGNED_RSH",
  "BIT_AND",
  "BIT_OR",
  "BIT_XOR",
  "BIT_NOT",
  "LOGICAL_AND",
  "LOGICAL_OR",
  "LOGICAL_NOT",
  "ASSIGN",
  "ADD_ASSIGN",
  "SUB_ASSIGN",
  "MUL_ASSIGN",
  "DIV_ASSIGN",
  "MOD_ASSIGN",

  "LSH_ASSIGN",
  "RSH_ASSIGN",
  "UNSIGNED_RSH_ASSIGN",
  "AND_ASSIGN",
  "OR_ASSIGN",
  "XOR_ASSIGN",
  "R_ARROW",
  "TK_NULL",
  "TK_BOOL",
  "NUMBER",
  "STRING",
  "TEMPLATE_STR",
  "REGEX",
  "LINE_TERM",
  "EOS",
  "NONE",
  "ILLEGAL",
};

#endif