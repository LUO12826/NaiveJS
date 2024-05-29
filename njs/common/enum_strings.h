#ifndef NJS_ENUM_STRINGS_H
#define NJS_ENUM_STRINGS_H

namespace njs {

/**
Ask chatGPT:
Please help me convert the following enumeration values into corresponding C string arrays,
so that I can easily retrieve their names using the enumeration values as subscripts.
Please ignore any comments, but keep the blank lines. Please do not add any new enum values.
*/


inline const char* ast_type_names[] = {
    "TOKEN",
    "BEGIN_EXPR",
    "EXPR_THIS",
    "EXPR_ID",
    "EXPR_STRICT_FUTURE",
    "EXPR_NULL",
    "EXPR_BOOL",
    "EXPR_NUMBER",
    "EXPR_STRING",
    "EXPR_REGEXP",
    "EXPR_ARRAY",
    "EXPR_OBJ",
    "EXPR_PAREN",
    "EXPR_BINARY",
    "EXPR_ASSIGN",
    "EXPR_UNARY",
    "EXPR_TRIPLE",
    "EXPR_ARGS",
    "EXPR_LHS",
    "EXPR_NEW",
    "EXPR_COMMA",
    "END_EXPR",
    "FUNC",
    "STMT_EMPTY",
    "STMT_BLOCK",
    "STMT_IF",
    "STMT_WHILE",
    "STMT_DO_WHILE",
    "STMT_FOR",
    "STMT_FOR_IN",
    "STMT_WITH",
    "STMT_TRY",
    "STMT_VAR",
    "STMT_VAR_DECL",
    "STMT_CONTINUE",
    "STMT_BREAK",
    "STMT_RETURN",
    "STMT_THROW",
    "STMT_SWITCH",
    "STMT_LABEL",
    "STMT_DEBUG",
    "PROGRAM",
    "FUNC_BODY",
    "ILLEGAL",
};

inline const char* token_type_names[] = {
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

inline const char* scope_type_names[] = {
    "GLOBAL_SCOPE",
    "FUNC_SCOPE",
    "FUNC_PARAM_SCOPE",
    "BLOCK_SCOPE",
    "CLOSURE_SCOPE"
};

inline const char* scope_type_names_alt[] = {
    "global",
    "local",
    "arg",
    "(error)",
    "closure"
};

inline const char* js_value_tag_names[] = {
    "UNDEFINED",
    "UNINIT",
    "JS_NULL",
    "JS_ATOM",
    "SYMBOL",
    "BOOLEAN",
    "NUM_UINT32",
    "NUM_INT32",
    "NUM_FLOAT",
    "VALUE_HANDLE",
    "PROC_META",
    "NEED_GC_BEGIN",
    "STRING",
    "HEAP_VAL",
    "OBJECT_BEGIN",
    "BOOLEAN_OBJ",
    "NUMBER_OBJ",
    "STRING_OBJ",
    "OBJECT",
    "ARRAY",
    "FUNCTION",
    "NEED_GC_END",
    "JSVALUE_TAG_CNT"
};

inline const char* object_class_names[] = {
    "CLS_OBJECT",
    "CLS_ARRAY",
    "CLS_STRING",
    "CLS_NUMBER",
    "CLS_BOOLEAN",
    "CLS_ERROR",
    "CLS_REGEXP",
    "CLS_DATE",
    "CLS_FUNCTION",
    "CLS_FOR_IN_ITERATOR",
    "CLS_ARRAY_ITERATOR",
    "CLS_CUSTOM",

    "CLS_OBJECT_PROTO",
    "CLS_ARRAY_PROTO",
    "CLS_NUMBER_PROTO",
    "CLS_BOOLEAN_PROTO",
    "CLS_STRING_PROTO",
    "CLS_FUNCTION_PROTO",
    "CLS_ERROR_PROTO",
    "CLS_REGEXP_PROTO",
    "CLS_ITERATOR_PROTO"
};


} // namespace njs

#endif // NJS_ENUM_STRINGS_H