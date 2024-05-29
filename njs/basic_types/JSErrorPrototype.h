#ifndef NJS_JS_ERROR_PROTOTYPE_H
#define NJS_JS_ERROR_PROTOTYPE_H

#include "JSObject.h"
#include "njs/vm/Completion.h"
#include "njs/common/common_def.h"
#include "njs/common/JSErrorType.h"

namespace njs {



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
