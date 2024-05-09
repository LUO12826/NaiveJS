#ifndef NJS_ENUM_STRINGS_H
#define NJS_ENUM_STRINGS_H

namespace njs {

inline const char *ast_type_names[44] = {
    "AST_TOKEN",

    "BEGIN_EXPR",

    "AST_EXPR_THIS",
    "AST_EXPR_ID",
    "AST_EXPR_STRICT_FUTURE",

    "AST_EXPR_NULL",
    "AST_EXPR_BOOL",
    "AST_EXPR_NUMBER",
    "AST_EXPR_STRING",
    "AST_EXPR_REGEXP",

    "AST_EXPR_ARRAY",
    "AST_EXPR_OBJ",

    "AST_EXPR_PAREN",  // ( Expression )

    "AST_EXPR_BINARY",
    "AST_EXPR_ASSIGN",
    "AST_EXPR_UNARY",
    "AST_EXPR_TRIPLE",

    "AST_EXPR_ARGS",
    "AST_EXPR_LHS",
    "AST_EXPR_NEW",

    "AST_EXPR_COMMA",

    "END_EXPR",

    "AST_FUNC",

    "AST_STMT_EMPTY",
    "AST_STMT_BLOCK",
    "AST_STMT_IF",
    "AST_STMT_WHILE",
    "AST_STMT_FOR",
    "AST_STMT_FOR_IN",
    "AST_STMT_WITH",
    "AST_STMT_DO_WHILE",
    "AST_STMT_TRY",

    "AST_STMT_VAR",
    "AST_STMT_VAR_DECL",

    "AST_STMT_CONTINUE",
    "AST_STMT_BREAK",
    "AST_STMT_RETURN",
    "AST_STMT_THROW",

    "AST_STMT_SWITCH",

    "AST_STMT_LABEL",
    "AST_STMT_DEBUG",

    "AST_PROGRAM",
    "AST_FUNC_BODY",

    "AST_ILLEGAL"
};

inline const char *token_type_names[74] = {
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

inline const char *scope_type_names[5] = {
    "GLOBAL_SCOPE",
    "FUNC_SCOPE",
    "FUNC_PARAM_SCOPE",
    "BLOCK_SCOPE",
    "CLOSURE_SCOPE"
};

inline const char *scope_type_names_alt[5] = {
    "global",
    "local",
    "arg",
    "(error)",
    "closure"
};

inline const char *js_value_tag_names[23] = {
    "UNDEFINED",
    "UNINIT",
    "JS_NULL",
    "JS_ATOM",
    "BOOLEAN",
    "NUM_UINT32",
    "NUM_INT32",
    "NUM_INT64",
    "NUM_FLOAT",
    "VALUE_HANDLE",
    "NEED_GC_BEGIN",
    "STRING",
    "SYMBOL",
    "HEAP_VAL",
    "OBJECT_BEGIN",
    "BOOLEAN_OBJ",
    "NUMBER_OBJ",
    "STRING_OBJ",
    "OBJECT",
    "ARRAY",
    "FUNCTION",
    "NEED_GC_END",
    "JSVALUE_TAG_CNT",
};

inline const char *object_class_names[15] = {
    "CLS_OBJECT",
    "CLS_ARRAY",
    "CLS_STRING",
    "CLS_NUMBER",
    "CLS_BOOLEAN",
    "CLS_ERROR",
    "CLS_DATE",
    "CLS_FUNCTION",
    "CLS_CUSTOM",

    "CLS_OBJECT_PROTO",
    "CLS_ARRAY_PROTO",
    "CLS_NUMBER_PROTO",
    "CLS_BOOLEAN_PROTO",
    "CLS_STRING_PROTO",
    "CLS_FUNCTION_PROTO",
};


} // namespace njs

#endif // NJS_ENUM_STRINGS_H