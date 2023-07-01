#ifndef NJS_LEXER_H
#define NJS_LEXER_H

#include <string>
#include <string_view>
#include <cstdio>
#include <cassert>

#include "njs/parser/character.h"
#include "njs/parser/token.h"
#include "njs/utils/helper.h"
#include "njs/include/SmallVector.h"

using std::u16string;
using std::u16string_view;

namespace njs {

using TokenType = Token::TokenType;
using u32 = uint32_t;
using llvm::SmallVector;

class Lexer {
 public:
  
  Lexer(std::u16string source): source(source),
                                cursor(0),
                                ch(source[0]),
                                length(source.size()) {}

  Lexer(const Lexer&) = delete;
  Lexer(Lexer&&) = delete;

  const Token& next() {

    if (peeking) {
      peeking = false;
      return curr_token;
    }

    line_term_before = false;
    // Token curr_token(TokenType::NONE, u"", 0, 0);
    curr_token.type = TokenType::NONE;
    do {
      u32 start = cursor;
      if (cursor == source.size()) {
        curr_token.set(TokenType::EOS, u"", start, start, curr_line);
        break;
      }
      switch (ch) {
        case u'{': {
          next_char();
          curr_token.set(TokenType::LEFT_BRACE, u"{", start, start + 1, curr_line);
          brace_stack.push_back(BraceType::LEFT);
          break;
        }
        case u'}': {
          next_char();
          curr_token.set(TokenType::RIGHT_BRACE, u"}", start, start + 1, curr_line);
          brace_stack.pop_back();
          break;
        }
        case u'(': {
          next_char();
          curr_token.set(TokenType::LEFT_PAREN, u"(", start, start + 1, curr_line);
          break;
        }
        case u')': {
          next_char();
          curr_token.set(TokenType::RIGHT_PAREN, u")", start, start + 1, curr_line);
          break;
        }
        case u'[': {
          next_char();
          curr_token.set(TokenType::LEFT_BRACK, u"[", start, start + 1, curr_line);
          break;
        }
        case u']': {
          next_char();
          curr_token.set(TokenType::RIGHT_BRACK, u"]", start, start + 1, curr_line);
          break;
        }
        case u';': {
          next_char();
          curr_token.set(TokenType::SEMICOLON, u";", start, start + 1, curr_line);
          break;
        }
        case u',': {
          next_char();
          curr_token.set(TokenType::COMMA, u",", start, start + 1, curr_line);
          break;
        }
        case u'?': {
          next_char();
          curr_token.set(TokenType::QUESTION, u"?", start, start + 1, curr_line);
          break;
        }
        case u':': {
          next_char();
          curr_token.set(TokenType::COLON, u":", start, start + 1, curr_line);
          break;
        }

        case u'.': {
          if (character::is_decimal_digit(peek_char())) {
            curr_token = scan_numeric_literal();
          }
          else {
            if (peek_char(1) == u'.' && peek_char(2) == u'.') {
              curr_token.set(TokenType::ELLIPSIS, u"...", start, start + 3, curr_line);
              next_char(3);
            }
            else {
              curr_token.set(TokenType::DOT, u".", start, start + 1, curr_line);
              next_char();
            }
            
          }
          break;
        }

        case u'<': {
          next_char();
          switch (ch) {
            case u'<':
              next_char();
              switch (ch) {
                case u'=':  // <<=
                  next_char();
                  curr_token.set(TokenType::LSH_ASSIGN, u"<<=", start, start + 3, curr_line);
                  break;
                default:  // <<
                  curr_token.set(TokenType::LSH, u"<<", start, start + 2, curr_line);
              }
              break;
            case u'=':  // <=
              next_char();
              curr_token.set(TokenType::LE, u"<=", start, start + 2, curr_line);
              break;
            default:  // <
              curr_token.set(TokenType::LT, u"<", start, start + 1, curr_line);
          }
          break;
        }

        case u'>': {
          next_char();
          switch (ch) {
            case u'>':
              next_char();
              switch (ch) {
                case u'>':
                  next_char();
                  switch (ch) {
                    case u'=':  // >>>=
                      next_char();
                      curr_token.set(TokenType::UNSIGNED_RSH_ASSIGN, u">>>=", start, start + 4, curr_line);
                      break;
                    default:  // >>>
                      curr_token.set(TokenType::UNSIGNED_RSH, u">>>", start, start + 3, curr_line);
                  }
                  break;
                case u'=':  // >>=
                  curr_token.set(TokenType::RSH_ASSIGN, u">>=", start, start + 3, curr_line);
                  next_char();
                  break;
                default:  // >>
                  curr_token.set(TokenType::RSH, u">>", start, start + 2, curr_line);
              }
              break;
            case u'=':  // >=
              next_char();
              curr_token.set(TokenType::GE, u">=", start, start + 2, curr_line);
              break;
            default:  // >
              curr_token.set(TokenType::GT, u">", start, start + 1, curr_line);
          }
          break;
        }

        case u'=': {
          next_char();
          switch (ch) {
            case u'=':
              next_char();
              switch (ch) {
                case u'=':  // ===
                  next_char();
                  curr_token.set(TokenType::EQ3, u"===", start, start + 3, curr_line);
                  break;
                default:  // ==
                  curr_token.set(TokenType::EQ, u"==", start, start + 2, curr_line);
                  break;
              }
              break;

            case u'>':  // =>
              next_char();
              curr_token.set(TokenType::R_ARROW, u"=>", start, start + 1, curr_line);
              break;
            default:  // =
              curr_token.set(TokenType::ASSIGN, u"=", start, start + 1, curr_line);
          }
          break;
        }
        
        case u'!': {
          next_char();
          switch (ch) {
            case u'=':
              next_char();
              switch (ch) {
                case u'=':  // !==
                  next_char();
                  curr_token.set(TokenType::NE3, u"!==", start, start + 3, curr_line);
                  break;
                default:  // !=
                  curr_token.set(TokenType::NE, u"!=", start, start + 2, curr_line);
                  break;
              }
              break;
            default:  // !
              curr_token.set(TokenType::LOGICAL_NOT, u"!", start, start + 1, curr_line);
          }
          break;
        }

        case u'+': {
          next_char();
          switch (ch) {
            case u'+':  // ++
              next_char();
              curr_token.set(TokenType::INC, u"++", start, start + 2, curr_line);
              break;
            case u'=':  // +=
              next_char();
              curr_token.set(TokenType::ADD_ASSIGN, u"+=", start, start + 2, curr_line);
              break;
            default:  // +
              curr_token.set(TokenType::ADD, u"+", start, start + 1, curr_line);
          }
          break;
        }
        
        case u'-': {
          next_char();
          switch (ch) {
            case u'-':  // --
              next_char();
              curr_token.set(TokenType::DEC, u"--", start, start + 2, curr_line);
              break;
            case u'=':  // -=
              next_char();
              curr_token.set(TokenType::SUB_ASSIGN, u"-=", start, start + 2, curr_line);
              break;
            default:  // -
              curr_token.set(TokenType::SUB, u"-", start, start + 1, curr_line);
          }
          break;
        }
        
        case u'*': {
          next_char();
          if (ch == u'=') {  // *=
            next_char();
            curr_token.set(TokenType::MUL_ASSIGN, u"*=", start, start + 2, curr_line);
          } else {  // +
            curr_token.set(TokenType::MUL, u"+", start, start + 1, curr_line);
          }
          break;
        }
        
        case u'%': {
          next_char();
          if (ch == u'=') {  // %=
            next_char();
            curr_token.set(TokenType::MOD_ASSIGN, u"%=", start, start + 2, curr_line);
          } else {  // %
            curr_token.set(TokenType::MOD, u"%", start, start + 1, curr_line);
          }
          break;
        }

        case u'&': {
          next_char();
          switch (ch) {
            case u'&':  // &&
              next_char();
              curr_token.set(TokenType::LOGICAL_AND, u"&&", start, start + 2, curr_line);
              break;
            case u'=':  // &=
              next_char();
              curr_token.set(TokenType::AND_ASSIGN, u"&=", start, start + 2, curr_line);
              break;
            default:  // &
              curr_token.set(TokenType::BIT_AND, u"&", start, start + 1, curr_line);
          }
          break;
        }
        
        case u'|': {
          next_char();
          switch (ch) {
            case u'|':  // ||
              next_char();
              curr_token.set(TokenType::LOGICAL_OR, u"||", start, start + 2, curr_line);
              break;
            case u'=':  // |=
              next_char();
              curr_token.set(TokenType::OR_ASSIGN, u"|=", start, start + 2, curr_line);
              break;
            default:  // |
              curr_token.set(TokenType::BIT_OR, u"|", start, start + 1, curr_line);
          }
          break;
        }
        
        case u'^': {
          next_char();
          if (ch == u'=') {  // ^=
            next_char();
            curr_token.set(TokenType::XOR_ASSIGN, u"^=", start, start + 2, curr_line);
          } else {
            curr_token.set(TokenType::BIT_XOR, u"^", start, start + 1, curr_line);
          }
          break;
        }
        
        case u'~': {
          next_char();
          curr_token.set(TokenType::BIT_NOT, u"~", start, start + 1, curr_line);
          break;
        }

        case u'/': {
          next_char();
          switch (ch) {
            case u'*':  // /*
              next_char();
              skip_multiline_comment();
              break;
            case u'/':  // //
              next_char();
              skip_singleline_comment();
              break;
            case u'=':  // /=
              next_char();
              curr_token.set(TokenType::DIV_ASSIGN, u"/=", start, start + 2, curr_line);
              break;
            default:  // /
              // We cannot distinguish DIV and regex in lexer level and therefore,
              // we need to check if the symbol of div operator or start of regex
              // in parser.
              curr_token.set(TokenType::DIV, u"/", start, start + 1, curr_line);
          }
          break;
        }

        case u'\'':
        case u'"': {
          curr_token = scan_string_literal();
          break;
        }

        default:
          if (character::is_white_space(ch)) {
            skip_whitespace();
          }
          else if (character::is_line_terminator(ch)) {
            skip_line_terminators();
            line_term_before = true;
          }
          else if (character::is_decimal_digit(ch)) {
            curr_token = scan_numeric_literal();
          }
          else if (character::is_identifier_start(ch)) {
            curr_token = scan_identifier();
          }
          else {
            next_char();
            curr_token.set(TokenType::ILLEGAL, view_of_source(start, start + 1), start, start + 1, curr_line);
          }
      }
    } while (curr_token.type == TokenType::NONE);
    // curr_token = curr_token;
    return curr_token;
  }

  const Token& peek() {
    if (peeking) {
      return curr_token;
    }
    checkpoint();
    next();
    peeking = true;
    
    return curr_token;
  }

  bool line_term_ahead() {
    if (peeking) {
      return line_term_before;
    }
    skip_whitespace();
    if (cursor == length) return true;
    if (character::is_line_terminator(ch)) return true;
    if (peek_char(1) == u'/' && peek_char(2) == u'/') return true;
    return false;
  }

  // Try skipping a semicolon. This is actually used to detect if a statement is finished,
  // so if there is a line break, it is also considered as meeting a semicolon. If the semicolon
  // is successfully skipped, the call of `next` will obtain the next nonblank token except semicolon
  // (it will skip semicolon).
  bool try_skip_semicolon() {

    if (!peeking) {
      checkpoint();
    }
    Token token = next();

    if (token.is_semicolon() || token.type == TokenType::EOS) {
      return true;
    }
    if (token.type == TokenType::RIGHT_BRACE || line_term_before) {
      back();
      return true;
    }
    back();
    return false;
  }

  bool scan_regexp_pattern(std::u16string& pattern) {

    assert(ch == u'/');
    next_char();
    if (!character::is_regular_expression_first_char(ch)) {
      next_char();
      return false;
    }
    while(cursor != source.size() && ch != u'/' && !character::is_line_terminator(ch)) {
      switch (ch) {
        case u'\\': {  // Regular Expression
          if (!skip_regexp_backslash_sequence(pattern)) {
            next_char();
            return false;
          }
          break;
        }
        case u'[': {
          if (!skip_regexp_class(pattern)) {
            next_char();
            return false;
          }
          break;
        }
        default:
          skip_regexp_chars(pattern);
      }
    }
    return true;
  }

  bool scan_regexp_flag(std::u16string& flag) {
    if (ch == u'/') {
      next_char();
      // RegularExpressionFlags
      while (character::legal_identifier_char(ch)) {
        if (ch == u'\\') {
          next_char();
          if (!skip_unicode_escape_sequence(flag)) {
            next_char();
            return false;
          }
        } else {
          flag += ch;
          next_char();
        }
      }
      return true;
    }
    return false;
  }

  Token scan_regexp_literal(std::u16string& pattern, std::u16string& flag) {
    if (peeking) {
      debug_printf("[warning] calling `scan_regexp_literal` while peeking.\n");
    }
    u32 start = cursor;
    if (!scan_regexp_pattern(pattern)) {
      goto error;
    }
    if (!scan_regexp_flag(flag)) {
      goto error;
    }
    return token_with_type(TokenType::REGEX, start);
error:
    curr_token.set(TokenType::ILLEGAL, view_of_source(start, cursor), start, cursor, curr_line);
    return curr_token;
  }

  // For regex
  inline void cursor_back() {
    if (peeking) {
      debug_printf("[warning] calling `cursor_back` while peeking.\n");
    }
    if (cursor == 0) return;
    cursor -= 1;
    ch = source[cursor];
  }

  inline const Token& current() {
    if (peeking) {
      return saved_state.curr_token;
    }
    return curr_token; 
  }
  inline const Token* current_token_ptr() { return &current(); }
  inline u32 current_pos() {
    return peeking ? saved_state.cursor : cursor;
  }

 private:

  void checkpoint() {
    saved_state = {cursor, ch, curr_token, line_term_before, curr_line, curr_line_start_pos};
  }

  void back() {
    if (curr_token.type == TokenType::LEFT_BRACE) brace_stack.pop_back();
    else if (curr_token.type == TokenType::RIGHT_BRACE) brace_stack.push_back(BraceType::LEFT);

    cursor = saved_state.cursor;
    ch = saved_state.ch;
    curr_token = saved_state.curr_token;
    line_term_before = saved_state.line_term_before;
    curr_line = saved_state.curr_line;
    curr_line_start_pos = saved_state.curr_line_start_pos;
  }

  inline char16_t peek_char(int distance = 1) {
    if (cursor + distance >= length) {
      // TODO(zhuzilin) distinguish EOS and \0.
      return character::EOS;
    }
    return source[cursor + distance];
  }

  inline void next_char(u32 step = 1) {
    if (cursor + step < length) {
      cursor += step;
      ch = source[cursor];
    }
    else {
      cursor = length;
      ch = character::EOS;
    }
  }

  inline std::u16string_view view_of_source(u32 start, u32 end) {
    return std::u16string_view(source.data() + start, end - start);
  }

  inline Token token_with_type(TokenType type, u32 start_pos) {
    return Token(type, view_of_source(start_pos, cursor), start_pos, cursor, curr_line);
  }

  bool skip_regexp_backslash_sequence(std::u16string& pattern) {
    assert(ch == u'\\');
    pattern += ch;
    next_char();
    if (character::is_line_terminator(ch)) {
      return false;
    }
    pattern += ch;
    next_char();
    return true;
  }

  void skip_regexp_chars(std::u16string& pattern) {
    while (cursor != source.size() && character::is_regular_expression_char(ch)) {
      pattern += ch;
      next_char();
    }
  }

  bool skip_regexp_class(std::u16string& pattern) {
    assert(ch == u'[');
    pattern += ch;
    next_char();
    while (cursor != source.size() && character::is_regular_expression_class_char(ch)) {
      switch (ch) {
        case u'\\': {
          if (!skip_regexp_backslash_sequence(pattern)) {
            return false;
          }
          break;
        }
        default:
          pattern += ch;
          next_char();
      }
    }
    if (ch == u']') {
      return true;
    }
    return false;
  }

  void skip_multiline_comment() {
    while (cursor != source.size()) {
      if (ch == u'*') {
        next_char();
        if (ch == u'/') {
          next_char();
          break;
        }
      }
      else if (character::is_line_terminator(ch)) {
        skip_line_terminators();
      }
      else {
        next_char();
      }
    }
  }

  void skip_singleline_comment() {
    // This will not skip line terminators.
    while (cursor != source.size() && !character::is_line_terminator(ch)) {
      next_char();
    }
  }

  void skip_whitespace() {
    while(character::is_white_space(ch)) {
      next_char();
    }
  }

  // Token scan_line_terminator_sequence() {
  //   assert(character::is_line_terminator(ch));
  //   u32 start = cursor;
  //   if (ch == character::CR && peek_char() == character::LF) {
  //     next_char(); next_char();
  //   } else {
  //     next_char();
  //   }
  //   return token_with_type(TokenType::LINE_TERM, start);
  // }

  void skip_line_terminators() {
    assert(character::is_line_terminator(ch));
    if (ch == character::CR && peek_char() == character::LF) {
      next_char(); next_char();
    } else {
      next_char();
    }

    curr_line += 1;
    curr_line_start_pos = cursor;
  }

  Token scan_string_literal() {
    char16_t quote = ch;
    u32 start = cursor;
    std::u16string tmp;
    next_char();
    while(cursor != source.size() && ch != quote && !character::is_line_terminator(ch)) {
      switch (ch) {
        case u'\\': {
          next_char();
          // TODO(zhuzilin) Find out if "\1" will trigger error.
          switch (ch) {
            case u'0': {
              next_char();
              if (character::is_decimal_digit(peek_char())) {
                next_char();
                goto error;
              }
              break;
            }
            case u'x': {  // HexEscapeSequence
              next_char();
              for (u32 i = 0; i < 2; i++) {
                if (!character::is_hex_digit(ch)) {
                  next_char();
                  goto error;
                }
                next_char();
              }
              break;
            }
            case u'u': {  // UnicodeEscapeSequence
              // TODO(zhuzilin) May need to interpret unicode here
              if (!skip_unicode_escape_sequence(tmp)) {
                next_char();
                goto error;
              }
              break;
            }
            default:
              if (character::is_line_terminator(ch)) {
                skip_line_terminators();
              } else if (character::is_char_escape_sequence(ch)) {
                next_char();
              } else {
                next_char();
                goto error;
              }
          }
          break;
        }
        default:
          next_char();
      }
    }

    if (ch == quote) {
      next_char();
      return token_with_type(TokenType::STRING, start);
    }
error:
    return token_with_type(TokenType::ILLEGAL, start);
  }

  void skip_decimal_digit() {
    while (character::is_decimal_digit(ch)) {
      next_char();
    }
  }

  bool skip_at_least_one_decimal_digit() {
    if (!character::is_decimal_digit(ch)) {
      return false;
    }
    while (character::is_decimal_digit(ch)) {
      next_char();
    }
    return true;
  }

  bool skip_at_least_one_hex_digit() {
    if (!character::is_hex_digit(ch)) {
      return false;
    }
    while (character::is_hex_digit(ch)) {
      next_char();
    }
    return true;
  }

  Token scan_numeric_literal() {
    assert(ch == u'.' || character::is_decimal_digit(ch));
    u32 start = cursor;

    bool is_hex = false;
    switch (ch) {
      case u'0': {
        next_char();
        switch (ch) {
          case u'x':
          case u'X': {  // HexIntegerLiteral
            next_char();
            if (!skip_at_least_one_hex_digit()) {
              next_char();
              goto error;
            }
            is_hex = true;
            break;
          }
          case u'.': {
            next_char();
            skip_decimal_digit();
            break;
          }
        }
        break;
      }
      case u'.': {
        next_char();
        if (!skip_at_least_one_decimal_digit()) {
          next_char();
          goto error;
        }
        break;
      }
      default:  // NonZeroDigit
        skip_at_least_one_decimal_digit();
        if (ch == u'.') {
          next_char();
          skip_decimal_digit();
        }
    }

    if(!is_hex) {  // ExponentPart
      if (ch == u'e' || ch == u'E') {
        next_char();
        if (ch == u'+' || ch == u'-') {
          next_char();
        }
        if (!skip_at_least_one_decimal_digit()) {
          next_char();
          goto error;
        }
      }
    }

    // The source character immediately following a NumericLiteral must not
    // be an IdentifierStart or DecimalDigit.
    if (character::is_identifier_start(ch) || character::is_decimal_digit(ch)) {
      next_char();
      goto error;
    }
    return token_with_type(TokenType::NUMBER, start);
error:
    return token_with_type(TokenType::ILLEGAL, start);
  }

  bool skip_unicode_escape_sequence(std::u16string& source) {
    if (ch != u'u') {
      return false;
    }
    next_char();
    char16_t c = 0;
    for (u32 i = 0; i < 4; i++) {
      if (!character::is_hex_digit(ch)) {
        return false;
      }
      c = c << 4 | character::u16_char_to_digit(ch);
      next_char();
    }
    source += c;
    return true;
  }

  Token scan_identifier() {
    assert(character::is_identifier_start(ch));
    u32 start = cursor;
    std::u16string id_text = u"";
    if (ch == u'\\') {
      next_char();
      if (!skip_unicode_escape_sequence(id_text)) {
        next_char();
        goto error;
      }
    } else {
      id_text += ch;
      next_char();
    }

    while (character::legal_identifier_char(ch)) {
      if (ch == u'\\') {
        next_char();
        if (!skip_unicode_escape_sequence(id_text)) {
          next_char();
          goto error;
        }
      } else {
        id_text += ch;
        next_char();
      }
    }

    if (id_text == u"null") {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::TK_NULL, start);
    }
    if (id_text == u"true" || id_text == u"false") {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::TK_BOOL, start);
    }
    if (keyword_set.count(id_text) > 0) {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::KEYWORD, start);
    }
    if (future_reserved_word_set.count(id_text) > 0) {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::FUTURE_KW, start);
    }
    if (strictmode_future_reserved_word_set.count(id_text) > 0) {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::STRICT_FUTURE_KW, start);
    }

    // IDENTIFIER
    return Token(TokenType::IDENTIFIER,
                  // insert(obj) return (inserted_obj, success_or_not)
                  // make a string view from the string that was put in the string pool.
                  u16string_view(string_pool.insert(id_text).first->c_str()),
                  start, cursor, curr_line);
error:
    return token_with_type(TokenType::ILLEGAL, start);
  }

  struct LexerState {
    u32 cursor;
    char16_t ch;
    Token curr_token {Token::none};
    bool line_term_before;

    u32 curr_line;
    u32 curr_line_start_pos;
  };

  enum class BraceType {
    LEFT = 0,
    DOLLAR_LEFT,
    RIGHT,
  };

  std::u16string source;

  u32 cursor;
  char16_t ch;
  u32 length;
  bool line_term_before {false};
  bool peeking {false};

  Token curr_token {Token::none};

  u32 curr_line {0};
  u32 curr_line_start_pos {0};

  LexerState saved_state;
  SmallVector<BraceType, 50> brace_stack;
  std::unordered_set<u16string> string_pool;
};

}  // namespace njs

#endif  // NJS_LEXER_H