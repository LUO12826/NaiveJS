#ifndef NJS_JS_ERRORTYPE_H
#define NJS_JS_ERRORTYPE_H

namespace njs {

enum JSErrorType {
  JS_ERROR,
  JS_EVAL_ERROR,
  JS_RANGE_ERROR,
  JS_REFERENCE_ERROR,
  JS_SYNTAX_ERROR,
  JS_TYPE_ERROR,
  JS_URI_ERROR,
  JS_INTERNAL_ERROR,
  JS_AGGREGATE_ERROR,

  JS_NATIVE_ERROR_COUNT
};

inline const char16_t *const native_error_name[JS_NATIVE_ERROR_COUNT] = {
    u"Error", u"EvalError", u"RangeError",
    u"ReferenceError", u"SyntaxError", u"TypeError",
    u"URIError",   u"InternalError",  u"AggregateError",
};

}

#endif // NJS_JS_ERRORTYPE_H
