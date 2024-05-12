#ifndef NJS_JSOBJECT_H
#define NJS_JSOBJECT_H

#include <cstdint>
#include <string>
#include <utility>

#include "JSObjectKey.h"
#include "njs/include/robin_hood.h"
#include "njs/utils/helper.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCObject.h"
#include "JSFunctionMeta.h"
#include "njs/vm/Completion.h"

namespace njs {

using u8 = uint8_t;
using std::u16string;
using std::u16string_view;
using robin_hood::unordered_flat_map;

class GCHeap;

// has corresponding string representation, note to modify when adding
enum class ObjClass {
  CLS_OBJECT = 0,
  CLS_ARRAY,
  CLS_STRING,
  CLS_NUMBER,
  CLS_BOOLEAN,
  CLS_ERROR,
  CLS_DATE,
  CLS_FUNCTION,
  CLS_CUSTOM,

  CLS_OBJECT_PROTO,
  CLS_ARRAY_PROTO,
  CLS_NUMBER_PROTO,
  CLS_BOOLEAN_PROTO,
  CLS_STRING_PROTO,
  CLS_FUNCTION_PROTO,
  CLS_ERROR_PROTO,
};

struct PropDesc {
  static constexpr u8 enumerable {1 << 0};
  static constexpr u8 E {1 << 0};
  static constexpr u8 configurable {1 << 1};
  static constexpr u8 C {1 << 1};
  static constexpr u8 writable {1 << 2};
  static constexpr u8 W {1 << 2};

  static constexpr u8 ECW {E | C | W};

  static constexpr u8 has_value {1 << 3};
  static constexpr u8 has_getter {1 << 4};
  static constexpr u8 has_setter {1 << 5};

  u8 flags {0};

  PropDesc(u8 flags = ECW): flags(has_value | flags) {}

  void set_ECW() { flags |= PropDesc::ECW; }

  bool is_value() const { return flags & has_value; }
  bool is_getset() const { return flags & (has_getter | has_setter); }
  bool is_enumerable() const { return flags & enumerable; }
  bool is_configurable() const { return flags & configurable; }
  bool is_writable() const { return flags & writable; }
};

class JSObject : public GCObject {
 public:

  struct JSObjectProp {
    PropDesc desc;
    union Data {
      JSValue value;
      struct {
        JSValue getter;
        JSValue setter;
      } getset;

      Data() {}

      Data& operator=(const Data& other) {
        getset = other.getset;
        return *this;
      }

      Data(const Data& other) {
        getset = other.getset;
      }

      Data(Data&& other) {
        getset = other.getset;
      }
    } data;

  };

  JSObject(): obj_class(ObjClass::CLS_OBJECT) {}
  explicit JSObject(ObjClass cls): obj_class(cls) {}
  explicit JSObject(ObjClass cls, JSValue proto): obj_class(cls), _proto_(proto) {}

  void gc_scan_children(GCHeap& heap) override;
  std::string description() override;
  virtual std::string to_string(NjsVM& vm) const;
  virtual void to_json(u16string& output, NjsVM& vm) const;

  virtual u16string_view get_class_name() { return u"Object"; }
  ObjClass get_class() { return obj_class; }


  void set_prototype(JSValue proto) {
    _proto_ = proto;
  }

  JSValue get_prototype() const {
    return _proto_;
  }

  // 7.1.1
  Completion to_primitive(NjsVM& vm, u16string_view preferred_type = u"default");
  Completion ordinary_to_primitive(NjsVM& vm, u16string_view hint);

  bool add_prop(const JSValue& key, const JSValue& value, PropDesc desc = PropDesc());
  bool add_prop(u32 key_atom, const JSValue& value, PropDesc desc = PropDesc());
  bool add_prop(NjsVM& vm, u16string_view key_str, const JSValue& value, PropDesc desc = PropDesc());
  bool add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl);

  JSValue get_prop(NjsVM& vm, u16string_view name);
  JSValue get_prop(NjsVM& vm, u16string_view name, bool get_ref);
  
  JSValue get_prop(u32 atom, bool get_ref) {
    return get_prop(JSValue::Atom(atom), get_ref);
  }

  JSValue get_prop(JSValue key, bool get_ref) {
    JSValue res = get_exist_prop(key, get_ref);
    if (res.tag != JSValue::UNINIT) {
      return res;
    }
    else if (get_ref) {
      JSValue& new_val = storage[JSObjectKey(key)].data.value;
      return JSValue(&new_val);
    }
    else {
      return JSValue::uninited;
    }
  }

  template <typename KEY>
  JSValue get_exist_prop(KEY&& key, bool get_ref) {
    auto res = storage.find(JSObjectKey(std::forward<KEY>(key)));
    if (res != storage.end()) {
      return get_ref ? JSValue(&(res->second.data.value)) : res->second.data.value;
    }
    else if (_proto_.is_object()) {
      return _proto_.as_object()->get_exist_prop(std::forward<KEY>(key), get_ref);
    }
    else {
      return JSValue::uninited;
    }
  }

  template <typename KEY>
  bool has_own_property(KEY&& key) {
    auto res = storage.find(JSObjectKey(std::forward<KEY>(key)));
    return res != storage.end();
  }

 private:
  ObjClass obj_class;
  unordered_flat_map<JSObjectKey, JSObjectProp> storage;
  JSValue _proto_;
};

} // namespace njs

#endif // NJS_JSOBJECT_H