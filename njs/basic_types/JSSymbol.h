#ifndef NJS_JS_SYMBOL_H
#define NJS_JS_SYMBOL_H

#include <string>
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCObject.h"
#include "njs/utils/helper.h"

namespace njs {

/// @brief https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol
struct JSSymbol: public GCObject {

  inline static size_t global_count {0};

  JSSymbol() = default;
  explicit JSSymbol(JSValue desc): desc(desc) {
    seq = JSSymbol::global_count;
    JSSymbol::global_count += 1;
  }

  bool operator == (const JSSymbol& other) const {
    return seq == other.seq;
  }

  void gc_scan_children(njs::GCHeap &heap) override {}

  std::string description() override {
    return to_string();
  }

  std::string to_string() {
    if (desc.is_undefined()) return "JSSymbol";
    else return "JSSymbol(" + to_u8string(desc.val.as_primitive_string->str) + ")";
  }

  JSValue desc;
  size_t seq;
};

} // namespace njs

/// @brief std::hash for JSSymbol. Injected into std namespace.
template <>
struct std::hash<njs::JSSymbol>
{
  std::size_t operator () (const njs::JSSymbol& symbol) const {
    return njs::hash_val(symbol.seq);
  }
};

#endif //NJS_JS_SYMBOL_H
