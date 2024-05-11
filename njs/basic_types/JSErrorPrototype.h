#ifndef NJS_JS_ERROR_PROTOTYPE_H
#define NJS_JS_ERROR_PROTOTYPE_H

#include "njs/common/common_def.h"
#include "njs/vm/Completion.h"
#include "JSObject.h"

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

class NjsVM;
class JSErrorPrototype : public JSObject {
 public:
  explicit JSErrorPrototype(NjsVM& vm, JSErrorType type = JS_ERROR);

  u16string_view get_class_name() override {
    return u"ErrorPrototype";
  }

  static Completion toString(JS_NATIVE_FUNC_PARAMS);
};

}

#endif //NJS_JS_ERROR_PROTOTYPE_H
