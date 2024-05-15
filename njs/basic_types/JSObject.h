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
#include "njs/vm/ErrorOr.h"

namespace njs {

using u8 = uint8_t;
using robin_hood::unordered_flat_map;
using std::u16string;
using std::u16string_view;

class GCHeap;


enum ObjClass {
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
  bool in_def_mode: 1 {false};

  bool enumerable: 1 {false};
  bool has_enum: 1 {false};
  bool configurable: 1 {false};
  bool has_config: 1 {false};
  bool writable: 1 {false};
  bool has_write: 1 {false};

  bool has_value: 1 {false};
  bool has_getter: 1 {false};
  bool has_setter: 1 {false};

  static PropFlag empty;
  static PropFlag VECW;

  // PropFlag() {}
  // PropFlag(bool e, bool c, bool w, bool v, bool g, bool s) :
  //   enumerable(e), configurable(c), writable(w),
  //   has_value(v), has_getter(g), has_setter(s) {}

  void set_ECW() {
    enumerable = configurable = writable = true;
  }

  bool is_value() const { return has_value; }
  bool is_getset() const { return has_getter | has_setter; }

  bool operator==(const PropFlag& other) const = default;
  bool operator!=(const PropFlag& other) const = default;
};

struct JSObjectKey {

  explicit JSObjectKey(u32 atom) : type(JSValue::STRING), atom(atom) {}

  explicit JSObjectKey(JSValue val) : type(val.tag), atom(val.val.as_atom) {
    assert(val.is_symbol() || val.is_atom());
  }

  bool operator==(const JSObjectKey& other) const {
    return atom == other.atom;
  }

  std::string to_string() const {
    if (type == JSValue::JS_ATOM) {
      return "Atom(" + std::to_string(atom) + ")";
    } else {
      return "Symbol(" + std::to_string(atom) + ")";
    }
  }

  u32 atom;
  JSValue::JSValueTag type;
};

}

namespace std {

/// @brief std::hash for JSObjectKey. Injected into std namespace.
template <>
struct std::hash<njs::JSObjectKey> {
  std::size_t operator()(const njs::JSObjectKey& obj_key) const {
    return obj_key.atom;
  }
};

} // namespace std
namespace njs {

struct JSObjectProp {
  PropFlag flag;
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
  
  bool is_data_descriptor() { return flag.is_value(); }
  bool is_accessor_descriptor() {return flag.has_getter || flag.has_setter; }
  bool is_generic_descriptor() {
    return !is_data_descriptor() && !is_accessor_descriptor();
  }
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

  JSObjectProp *get_own_prop(JSValue key) {
    assert(key.is_atom() || key.is_symbol());
    auto res = storage.find(JSObjectKey(key));
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

  Completion to_primitive(NjsVM& vm, u16string_view preferred_type = u"default");
  Completion ordinary_to_primitive(NjsVM& vm, u16string_view hint);

  /*
   * The following methods only accept atom or symbol as the key
   * if the key type is JSValue.
   */
  ErrorOr<bool> set_prop(NjsVM& vm, JSValue key, JSValue value, PropFlag flag = PropFlag::VECW);
  ErrorOr<bool> set_prop(NjsVM& vm, u32 key_atom, JSValue value, PropFlag flag = PropFlag::VECW);
  ErrorOr<bool> set_prop(NjsVM& vm, u16string_view key_str, JSValue value, PropFlag flag = PropFlag::VECW);
  bool add_prop_trivial(u32 key_atom, JSValue value);
  bool add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl);

  Completion get_prop(NjsVM& vm, u16string_view key_str);
  Completion get_prop(NjsVM& vm, JSValue key);
  Completion get_prop(NjsVM& vm, u32 key_atom) {
    return get_prop(vm, JSValue::Atom(key_atom));
  }

  JSValue get_prop_trivial(u32 key_atom) {
    JSObjectProp *prop = get_own_prop(JSValue::Atom(key_atom));
    if (prop == nullptr) [[unlikely]] {
      return JSValue::uninited;
    } else {
      assert(prop->is_data_descriptor());
      return prop->data.value;
    }
    
  }

  template <typename KEY>
  JSObjectProp *get_exist_prop(KEY&& key) {
    auto res = storage.find(JSObjectKey(std::forward<KEY>(key)));
    if (res != storage.end()) {
      return &res->second;
    } else if (_proto_.is_object()) {
      return _proto_.as_object()->get_exist_prop(std::forward<KEY>(key));
    } else {
      return nullptr;
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