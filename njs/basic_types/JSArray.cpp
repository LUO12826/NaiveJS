#include "JSArray.h"

namespace njs {

JSValue JSArray::access_element(u32 index, bool create_ref) {
  if (create_ref) {
    if (index >= dense_array.size()) dense_array.resize(long(index * 1.2));
    return JSValue(&dense_array[index]);
  }
  else {
    if (index >= dense_array.size()) return JSValue::undefined;
    return dense_array[index];
  }
}

}