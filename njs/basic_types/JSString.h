#ifndef NJS_JSSTRING_H
#define NJS_JSSTRING_H

#include "JSObject.h"
#include "PrimitiveString.h"
#include "njs/vm/NjsVM.h"
#include "njs/gc/GCHeap.h"

namespace njs {

class JSString : public JSObject {
 public:
  JSString(NjsVM& vm, PrimitiveString *str) :
      JSObject(vm, CLS_STRING, vm.string_prototype),
      value(str) {
    gc_write_barrier(value);
  }

  u16string_view get_class_name() override {
    return u"String";
  }

  bool gc_scan_children(njs::GCHeap &heap) override {
    bool child_young = false;
    child_young |= JSObject::gc_scan_children(heap);
    child_young |= heap.gc_visit_object(value);
    return child_young;
  }

  void gc_mark_children() override {
    JSObject::gc_mark_children();
    value->set_visited();
  }

  bool gc_has_young_child(GCObject *oldgen_start) override {
    return JSObject::gc_has_young_child(oldgen_start)
           || (value < oldgen_start);
  }

  Completion get_property_impl(NjsVM &vm, JSValue key) override {
    JSValue k = TRY_COMP(js_to_property_key(vm, key));
    if (k.is_atom() && k.as_atom == AtomPool::k_length) {
      return JSFloat(value->length());
    } else {
      return get_prop(vm, k);
    }
  }

  ErrorOr<bool> set_property_impl(NjsVM &vm, JSValue key, JSValue val) override {
    JSValue k = TRY_ERR(js_to_property_key(vm, key));
    if (k.is_atom() && k.as_atom == AtomPool::k_length) {
      return true;
    } else {
      return set_prop(vm, k, val);
    }
  }

  PrimitiveString* get_prim_value() {
    return value;
  }

//  std::string description() override;
//  std::string to_string(NjsVM& vm) override;
//  void to_json(u16string& output, NjsVM& vm) const override;
 private:
  PrimitiveString *value;
};

} // namespace njs

#endif //NJS_JSSTRING_H
