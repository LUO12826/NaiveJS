#ifndef NJS_LEXER_H
#define NJS_LEXER_H

#include <string>
#include <string_view>
#include <cassert>
#include <optional>
#include "lexing_helper.h"
#include "njs/include/SmallVector.h"
#include "njs/parser/Token.h"
#include "njs/parser/character.h"
#include "njs/utils/helper.h"

using std::u16string;
using std::u16string_view;

namespace njs {

using TokenType = Token::TokenType;
using u32 = uint32_t;
using std::u16string;
using std::optional;
using llvm::SmallVector;

class Lexer {
 public:
  
  explicit Lexer(u16string_view source): source(source), cursor(0),
                                           ch(source[0]), length(source.size()) {}

  Lexer(const Lexer&) = delete;
  Lexer(Lexer&&) = delete;

  const Token& next_twice() {
    next();
    return next();
  }

  const Token& next() {
#define token_source_expr(length) start, start + (length), start - curr_line_start + 1, curr_line
    if (peeking) {
      peeking = false;
      return curr_token;
    }

    line_term_before = false;
    curr_token.type = TokenType::NONE;
    do {
      u32 start = cursor;
      if (cursor == source.size()) [[unlikely]] {
        curr_token.set(TokenType::EOS, u"", token_source_expr(0));
        break;
      }
      switch (ch) {
        case u'{': {
          next_char();
          curr_token.set(TokenType::LEFT_BRACE, u"{", token_source_expr(1));
          brace_stack.push_back(BraceType::LEFT);
          break;
        }
        case u'}': {
          next_char();
          curr_token.set(TokenType::RIGHT_BRACE, u"}", token_source_expr(1));
          brace_stack.pop_back();
          break;
        }
        case u'(': {
          next_char();
          curr_token.set(TokenType::LEFT_PAREN, u"(", token_source_expr(1));
          break;
        }
        case u')': {
          next_char();
          curr_token.set(TokenType::RIGHT_PAREN, u")", token_source_expr(1));
          break;
        }
        case u'[': {
          next_char();
          curr_token.set(TokenType::LEFT_BRACK, u"[", token_source_expr(1));
          break;
        }
        case u']': {
          next_char();
          curr_token.set(TokenType::RIGHT_BRACK, u"]", token_source_expr(1));
          break;
        }
        case u';': {
          next_char();
          curr_token.set(TokenType::SEMICOLON, u";", token_source_expr(1));
          break;
        }
        case u',': {
          next_char();
          curr_token.set(TokenType::COMMA, u",", token_source_expr(1));
          break;
        }
        case u'?': {
          next_char();
          curr_token.set(TokenType::QUESTION, u"?", token_source_expr(1));
          break;
        }
        case u':': {
          next_char();
          curr_token.set(TokenType::COLON, u":", token_source_expr(1));
          break;
        }

        case u'.': {
          if (character::is_decimal_digit(peek_char())) {
            curr_token = scan_numeric_literal();
          }
          else {
            if (peek_char(1) == u'.' && peek_char(2) == u'.') {
              curr_token.set(TokenType::ELLIPSIS, u"...", token_source_expr(3));
              next_char(3);
            }
            else {
              curr_token.set(TokenType::DOT, u".", token_source_expr(1));
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
                  curr_token.set(TokenType::LSH_ASSIGN, u"<<=", token_source_expr(3));
                  break;
                default:  // <<
                  curr_token.set(TokenType::LSH, u"<<", token_source_expr(2));
              }
              break;
            case u'=':  // <=
              next_char();
              curr_token.set(TokenType::LE, u"<=", token_source_expr(2));
              break;
            default:  // <
              curr_token.set(TokenType::LT, u"<", token_source_expr(1));
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
                      curr_token.set(TokenType::UNSIGNED_RSH_ASSIGN, u">>>=", token_source_expr(4));
                      break;
                    default:  // >>>
                      curr_token.set(TokenType::UNSIGNED_RSH, u">>>", token_source_expr(3));
                  }
                  break;
                case u'=':  // >>=
                  curr_token.set(TokenType::RSH_ASSIGN, u">>=", token_source_expr(3));
                  next_char();
                  break;
                default:  // >>
                  curr_token.set(TokenType::RSH, u">>", token_source_expr(2));
              }
              break;
            case u'=':  // >=
              next_char();
              curr_token.set(TokenType::GE, u">=", token_source_expr(2));
              break;
            default:  // >
              curr_token.set(TokenType::GT, u">", token_source_expr(1));
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
                  curr_token.set(TokenType::EQ3, u"===", token_source_expr(3));
                  break;
                default:  // ==
                  curr_token.set(TokenType::EQ, u"==", token_source_expr(2));
                  break;
              }
              break;

            case u'>':  // =>
              next_char();
              curr_token.set(TokenType::R_ARROW, u"=>", token_source_expr(1));
              break;
            default:  // =
              curr_token.set(TokenType::ASSIGN, u"=", token_source_expr(1));
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
                  curr_token.set(TokenType::NE3, u"!==", token_source_expr(3));
                  break;
                default:  // !=
                  curr_token.set(TokenType::NE, u"!=", token_source_expr(2));
                  break;
              }
              break;
            default:  // !
              curr_token.set(TokenType::LOGICAL_NOT, u"!", token_source_expr(1));
          }
          break;
        }

        case u'+': {
          next_char();
          switch (ch) {
            case u'+':  // ++
              next_char();
              curr_token.set(TokenType::INC, u"++", token_source_expr(2));
              break;
            case u'=':  // +=
              next_char();
              curr_token.set(TokenType::ADD_ASSIGN, u"+=", token_source_expr(2));
              break;
            default:  // +
              curr_token.set(TokenType::ADD, u"+", token_source_expr(1));
          }
          break;
        }
        
        case u'-': {
          next_char();
          switch (ch) {
            case u'-':  // --
              next_char();
              curr_token.set(TokenType::DEC, u"--", token_source_expr(2));
              break;
            case u'=':  // -=
              next_char();
              curr_token.set(TokenType::SUB_ASSIGN, u"-=", token_source_expr(2));
              break;
            default:  // -
              curr_token.set(TokenType::SUB, u"-", token_source_expr(1));
          }
          break;
        }
        
        case u'*': {
          next_char();
          if (ch == u'=') {  // *=
            next_char();
            curr_token.set(TokenType::MUL_ASSIGN, u"*=", token_source_expr(2));
          } else {  // *
            curr_token.set(TokenType::MUL, u"*", token_source_expr(1));
          }
          break;
        }
        
        case u'%': {
          next_char();
          if (ch == u'=') {  // %=
            next_char();
            curr_token.set(TokenType::MOD_ASSIGN, u"%=", token_source_expr(2));
          } else {  // %
            curr_token.set(TokenType::MOD, u"%", token_source_expr(1));
          }
          break;
        }

        case u'&': {
          next_char();
          switch (ch) {
            case u'&':  // &&
              next_char();
              if (ch == u'=') {  // &&=
                next_char();
                curr_token.set(TokenType::LOGI_AND_ASSIGN, u"&&=", token_source_expr(3));
              } else {
                curr_token.set(TokenType::LOGICAL_AND, u"&&", token_source_expr(2));
              }
              break;
            case u'=':  // &=
              next_char();
              curr_token.set(TokenType::AND_ASSIGN, u"&=", token_source_expr(2));
              break;
            default:  // &
              curr_token.set(TokenType::BIT_AND, u"&", token_source_expr(1));
          }
          break;
        }
        
        case u'|': {
          next_char();
          switch (ch) {
            case u'|':  // ||
              next_char();
              if (ch == u'=') {  // ||=
                next_char();
                curr_token.set(TokenType::LOGI_OR_ASSIGN, u"||=", token_source_expr(3));
              } else {
                curr_token.set(TokenType::LOGICAL_OR, u"||", token_source_expr(2));
              }
              break;
            case u'=':  // |=
              next_char();
              curr_token.set(TokenType::OR_ASSIGN, u"|=", token_source_expr(2));
              break;
            default:  // |
              curr_token.set(TokenType::BIT_OR, u"|", token_source_expr(1));
          }
          break;
        }
        
        case u'^': {
          next_char();
          if (ch == u'=') {  // ^=
            next_char();
            curr_token.set(TokenType::XOR_ASSIGN, u"^=", token_source_expr(2));
          } else {
            curr_token.set(TokenType::BIT_XOR, u"^", token_source_expr(1));
          }
          break;
        }
        
        case u'~': {
          next_char();
          curr_token.set(TokenType::BIT_NOT, u"~", token_source_expr(1));
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
              skip_single_line_comment();
              break;
            case u'=':  // /=
              next_char();
              curr_token.set(TokenType::DIV_ASSIGN, u"/=", token_source_expr(2));
              break;
            default:  // /
              // We cannot distinguish DIV and regex in lexer level and therefore,
              // we need to check if the original_symbol of div operator or start of regex
              // in parser.
              curr_token.set(TokenType::DIV, u"/", token_source_expr(1));
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
            curr_token.set(TokenType::ILLEGAL, view_of_source(start, start + 1), token_source_expr(1));
          }
      }
    } while (curr_token.type == TokenType::NONE);
    // curr_token = curr_token;
    return curr_token;

#undef token_source_expr
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
  // is successfully skipped, the call of `next` will obtain the next non-blank token except semicolon
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
          if (!scan_unicode_escape_sequence(flag)) {
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
    curr_token.set(TokenType::ILLEGAL, view_of_source(start, cursor),
                   start, cursor, start - curr_line_start + 1, curr_line);
    return curr_token;
  }

  // For regex
  void cursor_back() {
    if (peeking) {
      debug_printf("[warning] calling `cursor_back` while peeking.\n");
    }
    if (cursor == 0) return;
    cursor -= 1;
    ch = source[cursor];
  }

  const Token& current() {
    if (peeking) {
      return saved_state.curr_token;
    }
    return curr_token; 
  }

  double get_number_val() const { return number_val; }

  u16string& get_string_val() { return string_val; }

  u32 current_pos() {
    return peeking ? saved_state.cursor : cursor;
  }

  SourceLoc current_src_pos() {
    u32 idx = peeking ? saved_state.cursor : cursor;
    u32 line = peeking ? saved_state.curr_line : curr_line;
    u32 line_start = peeking ? saved_state.curr_line_start : curr_line_start;
    return {line, idx - line_start + 1, idx};
  }

 private:
  void checkpoint() {
    saved_state = {cursor, ch, curr_token, line_term_before, curr_line, curr_line_start};
  }

  void back() {
    if (curr_token.type == TokenType::LEFT_BRACE) brace_stack.pop_back();
    else if (curr_token.type == TokenType::RIGHT_BRACE) brace_stack.push_back(BraceType::LEFT);

    cursor = saved_state.cursor;
    ch = saved_state.ch;
    curr_token = saved_state.curr_token;
    line_term_before = saved_state.line_term_before;
    curr_line = saved_state.curr_line;
    curr_line_start = saved_state.curr_line_start;
  }

  char16_t peek_char(int distance = 1) {
    if (cursor + distance >= length) {
      return character::EOS;
    }
    return source[cursor + distance];
  }

  void next_char(u32 step = 1) {
    cursor += step;
    if (cursor < length) [[likely]] {
      ch = source[cursor];
    } else {
      cursor = length;
      ch = character::EOS;
    }
  }

  void update_char() {
    if (cursor < length) {
      ch = source[cursor];
    } else {
      cursor = length;
      ch = character::EOS;
    }
  }

  u16string_view view_of_source(u32 start, u32 end) {
    return {source.data() + start, end - start};
  }

  Token token_with_type(TokenType type, u32 start_pos) {
    return Token(type, view_of_source(start_pos, cursor),
                 start_pos, cursor, start_pos - curr_line_start + 1, curr_line);
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

  void skip_single_line_comment() {
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

  void skip_line_terminators() {
    assert(character::is_line_terminator(ch));
    if (ch == character::LF) [[likely]] {
      next_char();
    } else if (ch == character::CR && peek_char() == character::LF) {
      next_char(2);
    }
    curr_line += 1;
    curr_line_start = cursor;
  }

  Token scan_string_literal() {
    u32 start = cursor;
    auto res = njs::scan_string_literal(source.data(), length, cursor, curr_line, curr_line_start);
    update_char();

    if (res.has_value()) {
      string_val = std::move(res.value());
      return token_with_type(TokenType::STRING, start);
    } else {
      return token_with_type(TokenType::ILLEGAL, start);
    }
  }


  Token scan_numeric_literal() {
    assert(ch == u'.' || character::is_decimal_digit(ch));

    u32 start = cursor;
    auto res = njs::scan_numeric_literal(source.data(), length, cursor);
    update_char();

    if (res.has_value()) [[likely]] {
      number_val = res.value();
      return token_with_type(TokenType::NUMBER, start);
    } else {
      return token_with_type(TokenType::ILLEGAL, start);
    }
  }

  bool scan_unicode_escape_sequence(std::u16string& str) {
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
    str += c;
    return true;
  }

  Token scan_identifier() {
    assert(character::is_identifier_start(ch));
    u32 start = cursor;
    std::u16string id_text;
    if (ch == u'\\') {
      next_char();
      if (!scan_unicode_escape_sequence(id_text)) {
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
        if (!scan_unicode_escape_sequence(id_text)) {
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
    if (keywords.contains(id_text)) {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::KEYWORD, start);
    }
    if (future_reserved_words.contains(id_text)) {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::FUTURE_KW, start);
    }
    if (strict_future_reserved_words.contains(id_text)) {
      if (cursor - start != id_text.size()) goto error;
      return token_with_type(TokenType::STRICT_FUTURE_KW, start);
    }

    // IDENTIFIER
    return Token(TokenType::IDENTIFIER,
                  // insert(obj) return (inserted_obj, success_or_not)
                  // make a string view from the string that was put in the string pool.
                  u16string_view(*string_pool.insert(id_text).first),
                  start, cursor, start - curr_line_start + 1, curr_line);
error:
    return token_with_type(TokenType::ILLEGAL, start);
  }

  struct LexerState {
    u32 cursor;
    char16_t ch;
    Token curr_token {Token::none};
    bool line_term_before;

    u32 curr_line;
    u32 curr_line_start;
  };

  enum class BraceType: uint8_t {
    LEFT = 0,
    DOLLAR_LEFT,
    RIGHT,
  };

  u16string_view source;

  u32 cursor;
  char16_t ch;
  u32 length;
  bool line_term_before {false};
  bool peeking {false};

  Token curr_token {Token::none};
  double number_val {0};
  u16string string_val {0};

  u32 curr_line {1};
  u32 curr_line_start {0};

  LexerState saved_state;
  SmallVector<BraceType, 50> brace_stack;
  std::unordered_set<u16string> string_pool;
};

}  // namespace njs

#endif  // NJS_LEXER_H