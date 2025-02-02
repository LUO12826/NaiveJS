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
#include "njs/common/Completion.h"
#include "njs/common/ErrorOr.h"

namespace njs {

using std::u16string;
using std::u16string_view;
using robin_hood::unordered_flat_map;

class GCHeap;

enum ObjClass: uint8_t {
  // has corresponding string representation, note to modify when adding
  CLS_OBJECT = 0,
  CLS_ASYNC_FUNC, // 0b01
  CLS_GENERATOR_FUNC, // 0b10
  CLS_ASYNC_GENERATOR_FUNC, // 0b11
  CLS_FUNCTION,
  CLS_BOUND_FUNCTION,
  CLS_ARRAY,
  CLS_STRING,
  CLS_NUMBER,
  CLS_BOOLEAN,
  CLS_ERROR,
  CLS_REGEXP,
  CLS_DATE,
  CLS_PROMISE,
  CLS_GENERATOR,
  CLS_FOR_IN_ITERATOR,
  CLS_ARRAY_ITERATOR,
  CLS_CUSTOM,

  CLS_OBJECT_PROTO,
  CLS_ARRAY_PROTO,
  CLS_NUMBER_PROTO,
  CLS_BOOLEAN_PROTO,
  CLS_STRING_PROTO,
  CLS_FUNCTION_PROTO,
  CLS_PROMISE_PROTO,
  CLS_GENERATOR_PROTO,
  CLS_ERROR_PROTO,
  CLS_REGEXP_PROTO,
  CLS_ITERATOR_PROTO,
};

enum ToPrimTypeHint {
  HINT_DEFAULT,
  HINT_STRING,
  HINT_NUMBER,
};

enum LazyPropKind: uint8_t {
  NOT_LAZY,
  LAZY_PROTOTYPE,
};

struct PFlag {
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
  // TODO: check if this matters when do the comparison
  LazyPropKind lazy_kind : 4 {NOT_LAZY};

  static PFlag empty;
  static PFlag VECW;
  static PFlag VECW_DEF;
  static PFlag VCW;
  static PFlag VCW_DEF;
  static PFlag V;
  static PFlag V_DEF;

  void set_ECW() {
    enumerable = configurable = writable = true;
  }

  bool is_value() const { return has_value; }
  bool is_getset() const { return has_getter | has_setter; }
  bool is_empty() {
    bool not_empty = has_enum || has_config || has_write
                     || has_value || has_getter || has_setter;
    return !not_empty;
  }

  void populate() {
    if (not in_def_mode) return;
    in_def_mode = false;
    writable = has_write & writable;
    configurable = has_config & configurable;
    enumerable = has_enum & enumerable;

    assert(has_value ^ (has_getter | has_setter));
  }

  void to_definition() {
    in_def_mode = true;
    has_enum = true;
    has_write = true;
    has_config = true;

    assert(has_value ^ (has_getter | has_setter));
  }

  bool operator==(const PFlag& other) const = default;
  bool operator!=(const PFlag& other) const = default;
};

struct JSObjectKey {

  explicit JSObjectKey(u32 atom) : type(JSValue::STRING), atom(atom) {}

  explicit JSObjectKey(JSValue val) : type(val.tag), atom(val.as_atom) {
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

struct ObjKeyHasher {
  std::size_t operator()(const JSObjectKey& obj_key) const {
    return obj_key.atom;
  }
};

struct JSPropDesc {
  PFlag flag;
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

  bool operator==(const JSPropDesc& that) const;

  bool is_data_descriptor() { return flag.has_value || flag.has_write; }
  bool is_accessor_descriptor() {return flag.has_getter || flag.has_setter; }
  bool is_generic_descriptor() {
    return !is_data_descriptor() && !is_accessor_descriptor();
  }
};

class JSObject : public GCObject {

friend class JSArray;
friend class JSForInIterator;
using StorageType = unordered_flat_map<JSObjectKey, JSPropDesc, ObjKeyHasher>;

 public:
  JSObject() : obj_class(CLS_OBJECT), _proto_(JSValue::null) {}
  explicit JSObject(ObjClass cls) : obj_class(cls), _proto_(JSValue::null) {}
  explicit JSObject(NjsVM& vm, ObjClass cls, JSValue proto);

  template <typename T>
  T *as() {
    return static_cast<T *>(this);
  }

  bool gc_scan_children(GCHeap& heap) override;
  void gc_mark_children() override;
  bool gc_has_young_child(GCObject *oldgen_start) override;
  std::string description() override;

  virtual std::string to_string(NjsVM& vm) const;
  virtual void to_json(u16string& output, NjsVM& vm) const;

  virtual u16string_view get_class_name() { return u"Object"; }
  ObjClass get_class() { return obj_class; }
  void set_class(ObjClass cls) { obj_class = cls; }

  bool is_direct_function() { return CLS_ASYNC_FUNC <= obj_class && obj_class <= CLS_FUNCTION; }
  bool is_bound_function() { return obj_class == CLS_BOUND_FUNCTION; }
  bool is_array() { return obj_class == CLS_ARRAY; }

  bool is_extensible() { return extensible; }
  void prevent_extensions() { extensible = false; }

  Completion to_primitive(NjsVM& vm, ToPrimTypeHint preferred_type = HINT_DEFAULT);
  Completion ordinary_to_primitive(NjsVM& vm, ToPrimTypeHint hint);

  Completion get_property(NjsVM& vm, JSValue key);
  ErrorOr<bool> set_property(NjsVM& vm, JSValue key, JSValue value);
  ErrorOr<bool> delete_property(JSValue key);

  virtual Completion get_property_impl(NjsVM& vm, JSValue key);
  virtual ErrorOr<bool> set_property_impl(NjsVM& vm, JSValue key, JSValue value);
  virtual ErrorOr<bool> delete_property_impl(JSValue key);

  virtual Completion has_own_property(NjsVM& vm, JSValue key);
  virtual Completion has_property(NjsVM& vm, JSValue key);

  template <typename KEY>
  bool has_own_property_atom(KEY&& key) {
    auto res = storage.find(JSObjectKey(std::forward<KEY>(key)));
    return res != storage.end();
  }

  bool set_proto(NjsVM&vm, JSValue proto);
  JSValue get_proto() const { return _proto_; }

  ErrorOr<bool> define_own_property(NjsVM& vm, JSValue key, JSPropDesc& desc);
  ErrorOr<bool> define_own_property_impl(NjsVM& vm, JSValue key,JSPropDesc *curr_desc,
                                         JSPropDesc& desc);

  /*
   * The following methods only accept atom or symbol as the key
   * if the key type is JSValue.
   */
  ErrorOr<bool> set_prop(NjsVM& vm, JSValue key, JSValue value);
  ErrorOr<bool> set_prop(NjsVM& vm, u16string_view key_str, JSValue value);

  bool add_prop_trivial(NjsVM& vm, u16string_view key_str, JSValue value, PFlag flag = PFlag::VCW);
  bool add_prop_trivial(NjsVM& vm, u32 key_atom, JSValue value, PFlag flag = PFlag::VCW);
  bool add_prop_trivial(NjsVM& vm, JSValue key, JSValue value, PFlag flag = PFlag::VCW);

  bool add_method(NjsVM& vm, u16string_view key_str, NativeFuncType funcImpl, PFlag flag = PFlag::VCW);
  bool add_symbol_method(NjsVM& vm, u32 symbol, NativeFuncType funcImpl, PFlag flag = PFlag::VCW);

  Completion get_prop(NjsVM& vm, JSValue key);
  Completion get_prop(NjsVM& vm, u32 key_atom);
  Completion get_prop(NjsVM& vm, u16string_view key_str);

  JSValue get_prop_trivial(u32 key_atom) {
    JSPropDesc *prop = get_own_property(JSAtom(key_atom));
    if (prop == nullptr) [[unlikely]] {
      return prop_not_found;
    } else {
      assert(prop->is_data_descriptor());
      return prop->data.value;
    }
  }

  template <typename KEY>
  JSPropDesc *get_exist_prop(KEY&& key) {
    JSObject *the_obj = this;
    while (true) {
      auto res = the_obj->storage.find(JSObjectKey(std::forward<KEY>(key)));
      if (res != the_obj->storage.end()) {
        return &res->second;
      } else if (the_obj->_proto_.is_object()) {
        the_obj = the_obj->_proto_.as_object;
      } else {
        return nullptr;
      }
    }
  }

  JSPropDesc *get_own_property(JSValue key) {
    assert(key.is_atom() || key.is_symbol());
    auto res = storage.find(JSObjectKey(key));
    return likely(res != storage.end()) ? &res->second : nullptr;
  }

  template <typename KEY>
  bool has_prop(KEY&& key) {
    return get_exist_prop(key) != nullptr;
  }

  ErrorOr<JSPropDesc> to_property_descriptor(NjsVM& vm);
  // only for internal use
  const StorageType& get_storage() {
    return storage;
  }

 private:
  ObjClass obj_class;
  StorageType storage;
  JSValue _proto_;

  bool extensible {true};
};

} // namespace njs


#endif // NJS_JSOBJECT_H