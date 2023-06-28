#include "JSValue.h"

#include "njs/basic_types/JSObject.h"
#include "njs/gc/GCObject.h"

namespace njs {

GCObject *JSValue::as_GCObject() {
  return static_cast<GCObject *>(val.as_object);
}

JSValue JSValue::add(JSValue& rhs) {
  if (this->is_int() && rhs.is_float()) {
    return JSValue(this->val.as_int + rhs.val.as_int);
  }
  else if (this->is_float() && rhs.is_float()) {
    return JSValue(this->val.as_float64 + rhs.val.as_float64);
  }
  else {
    assert(false);
  }
}

}