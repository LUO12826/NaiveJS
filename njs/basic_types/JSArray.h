#ifndef NJS_JSARRAY_H
#define NJS_JSARRAY_H

#include <vector>
#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "njs/common/StringPool.h"

namespace njs {

using u32 = uint32_t;

class JSArray: public JSObject {
 public:
  JSArray(): JSArray(0) {}

  explicit JSArray(int length): JSObject(ObjectClass::CLS_ARRAY) {
    add_prop(StringPool::ATOM_length, JSValue((double)length));
  }

  JSArray(NjsVM& vm, int length): JSObject(ObjectClass::CLS_ARRAY, vm.array_prototype) {
    add_prop(StringPool::ATOM_length, JSValue((double)length));
  }

  void gc_scan_children(GCHeap& heap) override;

  u16string_view get_class_name() override {
    return u"Array";
  }
  std::string description() override;
  std::string to_string(NjsVM& vm) override;
  void to_json(u16string& output, NjsVM& vm) const override;

  inline JSValue access_element(u32 index, bool create_ref);

  u32 get_length() {
    return (u32)get_prop(StringPool::ATOM_length, false).val.as_f64;
  }

  void set_length(u32 length) {
    get_prop(StringPool::ATOM_length, true).deref().val.as_f64 = double(length);
  }

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
