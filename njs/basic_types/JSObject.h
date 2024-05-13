#ifndef NJS_JSOBJECT_H
#define NJS_JSOBJECT_H

#include <cstdint>
#include <string>
#include <utility>

#include "JSFunctionMeta.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCObject.h"
#include "njs/include/robin_hood.h"
#include "njs/utils/helper.h"
#include "njs/utils/macros.h"
#include "njs/vm/Completion.h"

namespace njs {

using u8 = uint8_t;
using robin_hood::unordered_flat_map;
using std::u16string;
using std::u16string_view;

class GCHeap;


enum class ObjClass {
  // has corresponding string representation, note to modify when adding
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

struct PropFlag {
  static constexpr u8 enumerable {1 << 0};
  static constexpr u8 E {1 << 0};
  static constexpr u8 configurable {1 << 1};
  static constexpr u8 C {1 << 1};
  static constexpr u8 writable {1 << 2};
  static constexpr u8 W {1 << 2};

  static constexpr u8 ECW {E | C | W};

  static constexpr u8 has_value {1 << 3};
  static constexpr u8 V {1 << 3};
  static constexpr u8 has_getter {1 << 4};
  static constexpr u8 G {1 << 4};
  static constexpr u8 has_setter {1 << 5};
  static constexpr u8 S {1 << 5};

  u8 flags {0};

  static PropFlag empty;
  static PropFlag VECW;

  PropFlag(u8 flags = 0) : flags(flags) {}

  void set_ECW() { flags |= PropFlag::ECW; }

  bool is_value() const { return flags & has_value; }
  bool is_getset() const { return flags & (has_getter | has_setter); }
  bool has_get() const { return flags & has_getter; };
  bool has_set() const { return flags & has_setter; };
  bool is_enumerable() const { return flags & enumerable; }
  bool is_configurable() const { return flags & configurable; }
  bool is_writable() const { return flags & writable; }

  bool operator==(const PropFlag& other) const { return flags == other.flags; }
  bool operator!=(const PropFlag& other) const { return flags != other.flags; }
};

struct JSObjectKey {
  explicit JSObjectKey(u32 atom) : key(JSValue::JS_ATOM) {
    key.val.as_atom = atom;
  }

  explicit JSObjectKey(JSValue val) : key(val) {
    assert(val.is(JSValue::SYMBOL) || val.is(JSValue::JS_ATOM));
  }

  bool operator==(const JSObjectKey& other) const {
    return key.val.as_atom == other.key.val.as_atom;
  }

  std::string to_string() const {
    if (key.is(JSValue::JS_ATOM)) {
      return "Atom(" + std::to_string(key.val.as_atom) + ")";
    } else {
      return "Symbol(" + std::to_string(key.val.as_symbol) + ")";
    }
  }

  JSValue key;
};

}

namespace std {

/// @brief std::hash for JSObjectKey. Injected into std namespace.
template <>
struct std::hash<njs::JSObjectKey> {
  std::size_t operator()(const njs::JSObjectKey& obj_key) const {
    return obj_key.key.val.as_atom;
  }
};

} // namespace std
namespace njs {

struct JSObjectProp {
  PropFlag desc;
  union Data {
    JSValue value;
    struct {
      JSValue getter;
      JSValue setter;
    } getset;

    Data() {}
    Data(const Data& other) { getset = other.getset; }
    Data(Data&& other) { getset = other.getset; }

    Data& operator=(const Data& other) {
      getset = other.getset;
      return *this;
    }
  } data;

  bool operator==(const JSObjectProp& other) const;
};

class JSObject : public GCObject {
 public:
  JSObject() : obj_class(ObjClass::CLS_OBJECT) {}
  explicit JSObject(ObjClass cls) : obj_class(cls) {}
  explicit JSObject(ObjClass cls, JSValue proto) : obj_class(cls), _proto_(proto) {}

  void gc_scan_children(GCHeap& heap) override;
  std::string description() override;
  
  virtual std::string to_string(NjsVM& vm) const;
  virtual void to_json(u16string& output, NjsVM& vm) const;

  virtual u16string_view get_class_name() { return u"Object"; }
  ObjClass get_class() { return obj_class; }

  bool set_proto(JSValue proto) {
    assert(proto.is_object() || proto.is_null());

    if (proto.is_null() && _proto_.is_null()) return true;
    if (proto.is_object() && _proto_.is_object()
        && proto.as_object() == _proto_.as_object()) {
      return true;
    }
    if (not extensible) return false;

    JSObject *p = proto.as_object_or_null();

    // check if there is a circular prototype chain
    while (p != nullptr) {
      if (p == this) return false;
      p = p->get_proto().as_object_or_null();
    }

    _proto_ = proto;
    return true;
  }

  JSValue get_proto() const { return _proto_; }

  bool is_extensible() { return extensible; }
  void prevent_extensions() { extensible = false; }

  JSObjectProp *get_own_prop(u32 key_atom) {
    auto res = storage.find(JSObjectKey(key_atom));
    return likely(res != storage.end()) ? &res->second : nullptr;
  }

  // bool define_own_prop(u32 key_atom, JSObjectProp& prop_desc) {
  //   JSObjectKey key(key_atom);
  //   auto find_res = storage.find(key);
  //   if (unlikely(find_res == storage.end())) {
  //     if (likely(extensible)) {
  //       storage[key] = prop_desc;
  //     } else {
  //       return false;
  //     }
  //   } else {
  //     if (prop_desc.is_empty()) return true;

  //     auto& curr = find_res->second;
  //     if (curr == prop_desc) return true;

  //   }
  // }

  // 7.1.1
  Completion to_primitive(NjsVM& vm, u16string_view preferred_type = u"default");
  Completion ordinary_to_primitive(NjsVM& vm, u16string_view hint);

  bool add_prop(const JSValue& key, const JSValue& value, PropFlag desc = PropFlag::VECW);
  bool add_prop(u32 key_atom, const JSValue& value, PropFlag desc = PropFlag::VECW);
  bool add_prop(NjsVM& vm, u16string_view key_str, const JSValue& value,
                PropFlag desc = PropFlag::VECW);
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

  bool extensible {true};
};

} // namespace njs


#endif // NJS_JSOBJECT_H