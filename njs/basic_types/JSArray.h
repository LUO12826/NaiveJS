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
  JSArray(): JSArray(0) {}
  
  explicit JSArray(int length): JSObject(ObjectClass::CLS_ARRAY) {
    JSValue length_atom(JSValue::JS_ATOM);
    length_atom.val.as_int64 = 0;
    add_prop(length_atom, JSValue((double)length));
  }

  void gc_scan_children(GCHeap& heap) override;

  std::string description() override;

  JSValue access_element(u32 index, bool create_ref);


  std::vector<JSValue> dense_array;
};

}

#endif // NJS_JSARRAY_H
