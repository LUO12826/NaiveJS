#ifndef NJS_JS_GENERATOR_H
#define NJS_JS_GENERATOR_H

#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "JSFunctionMeta.h"

namespace njs {

class JSGenerator : public JSObject {
 public:
  JSGenerator(NjsVM& vm) : JSObject(CLS_GENERATOR) {}
  ~JSGenerator() override {
    if (exec_state) {
      free(exec_state);
    }
  }

  u16string_view get_class_name() override {
    return u"Generator";
  }

  std::string description() override {
    return "Generator";
  }

  void dispose_exec_state() {
    assert(exec_state);
    free(exec_state);
    exec_state = nullptr;
  }

  bool gc_scan_children(GCHeap& heap) override {
    bool child_young = false;
    child_young |= JSObject::gc_scan_children(heap);
    if (exec_state) {
      child_young |= exec_state->gc_scan_children(heap);
    }
    return child_young;
  }

  void gc_mark_children() override {
    JSObject::gc_mark_children();
    if (exec_state) {
      exec_state->gc_mark_children();
    }
  }

  bool gc_has_young_child(GCObject *oldgen_start) override {
    if (JSObject::gc_has_young_child(oldgen_start)) return true;
    if (exec_state && exec_state->gc_has_young_child(oldgen_start)) {
      return true;
    }
    return false;
  }

  bool done {false};
  ResumableFuncState *exec_state {nullptr};
};

}

#endif // NJS_JS_GENERATOR_H
