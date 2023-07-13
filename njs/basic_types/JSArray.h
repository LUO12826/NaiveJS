#ifndef NJS_JSARRAY_H
#define NJS_JSARRAY_H

#include <vector>
#include "JSObject.h"
#include "njs/include/SmallVector.h"

namespace njs {

using llvm::SmallVector;
using u32 = uint32_t;

class JSArray: public JSObject {
 public:
  JSArray(): JSObject(ObjectClass::CLS_ARRAY) {}

  JSValue access_element(u32 index, bool create_ref);


  std::vector<JSValue> dense_array;
};

}

#endif // NJS_JSARRAY_H
