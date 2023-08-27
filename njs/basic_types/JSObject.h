#ifndef NJS_JSOBJECT_H
#define NJS_JSOBJECT_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "JSObjectKey.h"
#include "njs/include/robin_hood.h"
#include "njs/utils/helper.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCObject.h"
#include "RCObject.h"
#include "JSFunctionMeta.h"

namespace njs {

using std::u16string;
using std::u16string_view;
using robin_hood::unordered_flat_map;

class GCHeap;

// has corresponding string representation, note to modify when adding
enum class ObjectClass {
  CLS_OBJECT = 0,
  CLS_ARRAY,
  CLS_ERROR,
  CLS_DATE,
  CLS_FUNCTION,
  CLS_GLOBAL_OBJ,
  CLS_CUSTOM,

  CLS_OBJECT_PROTO,
  CLS_ARRAY_PROTO,
  CLS_FUNCTION_PROTO,
};

class JSObject : public GCObject {
 public:
  JSObject(): obj_class(ObjectClass::CLS_OBJECT) {}
  explicit JSObject(ObjectClass cls): obj_class(cls) {}
  explicit JSObject(ObjectClass cls, JSValue proto): obj_class(cls), _proto_(proto) {}

  ~JSObject() override;

  void gc_scan_children(GCHeap& heap) override;

  virtual u16string_view get_class_name() {
    return u"Object";
  }

  std::string description() override;
  virtual std::string to_string(NjsVM& vm);
  virtual void to_json(u16string& output, NjsVM& vm) const;

  void set_prototype(JSValue proto) {
    _proto_ = proto;
  }

  bool add_prop(const JSValue& key, const JSValue& value);
  bool add_prop(int64_t key_atom, const JSValue& value);
  bool add_prop(NjsVM& vm, u16string_view key_str, const JSValue& value);
  bool add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl);

  JSValue get_prop(NjsVM& vm, u16string_view name);
  JSValue get_prop(NjsVM& vm, u16string_view name, bool get_ref);

  template <typename KEY>
  JSValue get_prop(KEY&& key, bool get_ref) {
    JSValue res = get_exist_prop(std::forward<KEY>(key), get_ref);
    if (res.tag != JSValue::UNDEFINED) {
      return res;
    }
    else if (get_ref) {
      return JSValue(&storage[JSObjectKey(std::forward<KEY>(key))]);
    }
    else {
      return JSValue::undefined;
    }
  }

  template <typename KEY>
  JSValue get_exist_prop(KEY&& key, bool get_ref) {

    auto res = storage.find(JSObjectKey(std::forward<KEY>(key)));
    if (res != storage.end()) {
      if (get_ref) return JSValue(&res->second);
      else return res->second;
    }
    else if (_proto_.is_object()) {
      return _proto_.as_object()->get_exist_prop(std::forward<KEY>(key), get_ref);
    }
    else {
      return JSValue::undefined;
    }
  }

  ObjectClass obj_class;
  unordered_flat_map<JSObjectKey, JSValue> storage;
  JSValue _proto_;
};

} // namespace njs

#endif // NJS_JSOBJECT_H