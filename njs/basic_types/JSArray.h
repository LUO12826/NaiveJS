#ifndef NJS_JSARRAY_H
#define NJS_JSARRAY_H

#include <vector>
#include "JSObject.h"
#include "njs/common/StringPool.h"
#include "njs/include/SmallVector.h"

namespace njs {

using llvm::SmallVector;
using u32 = uint32_t;

class JSArray: public JSObject {
 public:
  JSArray(): JSArray(0) {}
  
  explicit JSArray(int length): JSObject(ObjectClass::CLS_ARRAY) {
    JSValue length_atom(JSValue::JS_ATOM);
    length_atom.val.as_int64 = StringPool::ATOM_length;
    add_prop(length_atom, JSValue((double)length));
  }

  void gc_scan_children(GCHeap& heap) override;

  std::string description() override;
  void to_json(u16string& output, NjsVM& vm) const;

  inline JSValue access_element(u32 index, bool create_ref);


  std::vector<JSValue> dense_array;
};

inline JSValue JSArray::access_element(u32 index, bool create_ref) {
  if (!create_ref) {
    if (index < dense_array.size()) return dense_array[index];
    return JSValue::undefined;
  }
  else {
    if (index < dense_array.size()) return JSValue(&dense_array[index]);
    dense_array.resize(long(index * 1.2));
    return JSValue(&dense_array[index]);
  }
}

}

#endif // NJS_JSARRAY_H
