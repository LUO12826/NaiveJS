#ifndef NJS_JSFUNCTION_META_H
#define NJS_JSFUNCTION_META_H

#include <vector>
#include "JSValue.h"
#include "njs/vm/Completion.h"
#include "njs/include/SmallVector.h"
#include "njs/common/ArrayRef.h"
#include "njs/common/common_def.h"
#include "njs/codegen/CatchTableEntry.h"

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;
using llvm::SmallVector;
using std::vector;

class GCHeap;
class NjsVM;
class JSFunction;

// Native function type. A Native function should act like a JavaScript function,
// accepting an array of arguments and returning a value.
using NativeFuncType = Completion(*)(NjsVM&, JSFunction&, JSValue, ArrayRef<JSValue>, CallFlags);

struct JSFunctionMeta {

  u32 name_index;
  bool is_anonymous {false};
  bool is_arrow_func {false};
  bool is_native {false};
  bool is_strict {false};
  bool prepare_arguments_array {false};

  u16 param_count;
  u16 local_var_count;
  u16 stack_size;
  u32 bytecode_start;
  u32 bytecode_end;

  u32 source_line;

  vector<CatchTableEntry> catch_table;

  NativeFuncType native_func {nullptr};
  int magic;

  std::string description() const;
};

inline JSFunctionMeta build_func_meta(NativeFuncType func) {
  return JSFunctionMeta {
      .is_anonymous = true,
      .is_native = true,
      .param_count = 0,
      .local_var_count = 0,
      .native_func = func,
  };
}

}

#endif //NJS_JSFUNCTION_META_H
