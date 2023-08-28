#ifndef NJS_JSFUNCTION_META_H
#define NJS_JSFUNCTION_META_H

#include "JSValue.h"
#include "njs/include/SmallVector.h"
#include "njs/common/ArrayRef.h"
#include "njs/codegen/CatchTableEntry.h"

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;
using llvm::SmallVector;

class GCHeap;
class NjsVM;
class JSFunction;

// Native function type. A Native function should act like a JavaScript function,
// accepting an array of arguments and returning a value.
using NativeFuncType = JSValue(*)(NjsVM&, JSFunction&, ArrayRef<JSValue>);

struct JSFunctionMeta {

  u32 name_index;
  bool is_anonymous {false};
  bool is_arrow_func {false};
  bool is_native {false};

  u16 param_count;
  u16 local_var_count;
  u32 code_address;

  u32 source_line;

  SmallVector<CatchTableEntry, 3> catch_table;

  NativeFuncType native_func {nullptr};

  std::string description() const;
};

}

#endif //NJS_JSFUNCTION_META_H
