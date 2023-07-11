#ifndef NJS_NATIVE_FUNCTION_H
#define NJS_NATIVE_FUNCTION_H

#include "njs/basic_types/JSValue.h"
#include "njs/common/ArrayRef.h"


namespace njs {

class JSObject;

class NjsVM;

void add_basic_functions(NjsVM& vm);

JSValue log(NjsVM& vm, ArrayRef<JSValue> args);

}

#endif // NJS_NATIVE_FUNCTION_H
