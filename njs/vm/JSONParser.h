#ifndef NJS_JSON_PARSER_H
#define NJS_JSON_PARSER_H

#include <string>
#include "njs/basic_types/JSValue.h"
#include "njs/basic_types/JSArray.h"
#include "njs/parser/lexing_helper.h"
#include "njs/vm/NjsVM.h"

namespace njs {

using std::u16string_view;

enum class JSONTokenType {
  STRING,
  NUMBER,
  BOOLEAN,
  JSON_NULL,
  LEFT_BRACE,
  RIGHT_BRACE,
  LEFT_BRACKET,
  RIGHT_BRACKET,
  COMMA,
  COLON,
  INVALID,
  END,
};

class NjsVM;

class JSONParser {
 public:
  JSONParser(NjsVM& vm, u16string_view json_str)
      : vm(vm), json_str(json_str), length(json_str.size()) {}

  Completion parse() {
    auto res = parse_value();
    auto token_type = next_token();
    if (token_type != JSONTokenType::END) {
      return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
    }
    return res;
  }

  u16string string_val;
  double number_val;
  bool boolean_val;

 private:
  JSONTokenType next_token() {
    while (cursor < length) {
      char16_t c = json_str[cursor];
      switch (c) {
        case u' ':
        case u'\t':
          cursor++;
          break;
        case u'\r':
          if (cursor + 1 < length && json_str[cursor + 1] == u'\n') {
            cursor += 1;
          }
          // fall through
        case u'\n':
          cursor += 1;
          curr_line += 1;
          curr_line_start = cursor;
          break;
        case u'{':
          cursor++;
          return JSONTokenType::LEFT_BRACE;
        case u'}':
          cursor++;
          return JSONTokenType::RIGHT_BRACE;
        case u'[':
          cursor++;
          return JSONTokenType::LEFT_BRACKET;
        case u']':
          cursor++;
          return JSONTokenType::RIGHT_BRACKET;
        case u',':
          cursor++;
          return JSONTokenType::COMMA;
        case u':':
          cursor++;
          return JSONTokenType::COLON;
        case u'"': {
          u32 line, line_start; // dummy variables
          auto res = scan_string_literal(json_str.data(), length, cursor, line, line_start);
          if (res.has_value()) {
            string_val = res.value();
            return JSONTokenType::STRING;
          } else {
            return JSONTokenType::INVALID;
          }
        }
        case u'0':
        case u'1':
        case u'2':
        case u'3':
        case u'4':
        case u'5':
        case u'6':
        case u'7':
        case u'8':
        case u'9': {
          auto res = scan_numeric_literal(json_str.data(), length, cursor);
          if (res.has_value()) {
            number_val = res.value();
            return JSONTokenType::NUMBER;
          } else {
            return JSONTokenType::INVALID;
          }
        }
        case u'-': {
          cursor += 1;
          auto res = scan_numeric_literal(json_str.data(), length, cursor);
          if (res.has_value()) {
            number_val = -res.value();
            return JSONTokenType::NUMBER;
          } else {
            return JSONTokenType::INVALID;
          }
        }
        case u't':
          if (cursor + 4 <= length && json_str.substr(cursor, 4) == u"true") {
            cursor += 4;
            boolean_val = true;
            return JSONTokenType::BOOLEAN;
          } else {
            return JSONTokenType::INVALID;
          }
        case u'f':
          if (cursor + 5 <= length && json_str.substr(cursor, 5) == u"false") {
            cursor += 5;
            boolean_val = false;
            return JSONTokenType::BOOLEAN;
          } else {
            return JSONTokenType::INVALID;
          }
        case u'n':
          if (cursor + 4 <= length && json_str.substr(cursor, 4) == u"null") {
            cursor += 4;
            return JSONTokenType::JSON_NULL;
          } else {
            return JSONTokenType::INVALID;
          }
        default:
          return JSONTokenType::INVALID;
      }
    }
    return JSONTokenType::END;
  }

  Completion parse_value() {
    auto token_type = next_token();
    return parse_value_inplace(token_type);
  }

  Completion parse_value_inplace(JSONTokenType token_type) {
    switch (token_type) {
      case JSONTokenType::LEFT_BRACE:
        return parse_object();
      case JSONTokenType::LEFT_BRACKET:
        return parse_array();
      case JSONTokenType::STRING:
        return vm.new_primitive_string(string_val);
      case JSONTokenType::NUMBER:
        return JSValue(number_val);
      case JSONTokenType::BOOLEAN:
        return JSValue(boolean_val);
      case JSONTokenType::JSON_NULL:
        return JSValue::null;
      default:
        return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
    }
  }

  Completion parse_object() {
    JSObject *obj = vm.new_object();
    JSONTokenType token_type = next_token();

    if (token_type == JSONTokenType::RIGHT_BRACE) {
      return JSValue(obj);
    }

    while (true) {
      // parse key
      if (token_type != JSONTokenType::STRING) {
        return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
      }
      auto key_atom = vm.str_to_atom(string_val);

      token_type = next_token();
      if (token_type != JSONTokenType::COLON) {
        return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
      }
      // parse value
      auto val = parse_value();
      if (val.is_error()) {
        return val;
      }
      obj->add_prop_trivial(vm, key_atom, val.get_value(), PFlag::VECW);
      token_type = next_token();
      if (token_type == JSONTokenType::COMMA) {
        token_type = next_token();
        if (token_type == JSONTokenType::RIGHT_BRACE) {
          return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
        }
      } else if (token_type == JSONTokenType::RIGHT_BRACE) {
        break;
      } else {
        return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
      }
    }

    return JSValue(obj);
  }

  Completion parse_array() {
    auto *arr = vm.heap.new_object<JSArray>(vm, 0);
    JSONTokenType token_type = next_token();

    if (token_type == JSONTokenType::RIGHT_BRACKET) {
      return JSValue(arr);
    }

    while (true) {
      auto val = parse_value_inplace(token_type);
      if (val.is_error()) return val;
      arr->push(vm, val.get_value());

      token_type = next_token();
      if (token_type == JSONTokenType::COMMA) {
        token_type = next_token();
        if (token_type == JSONTokenType::RIGHT_BRACKET) {
          return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
        }
      } else if (token_type == JSONTokenType::RIGHT_BRACKET) {
        break;
      } else {
        return vm.throw_error(JS_SYNTAX_ERROR, u"Invalid JSON");
      }
    }

    return JSValue(arr);
  }

  NjsVM& vm;
  u16string_view json_str;
  u32 cursor {0};
  u32 length;
  u32 curr_line {1};
  u32 curr_line_start {0};
};

}



#endif // NJS_JSON_PARSER_H
