#ifndef NJS_NJSVM_H
#define NJS_NJSVM_H

#include <cstdint>
#include <memory>

#include "njs/include/SmallVector.h"
#include "njs/basic_types/JSValue.h"
#include "njs/basic_types/JSFunction.h"


namespace njs {

using u32 = uint32_t;
using llvm::SmallVector;


class NjsVM {
 public:
  NjsVM() {
    rt_stack = std::make_unique<JSValue[]>(max_stack_size);
  }

 private:
  std::unique_ptr<JSValue[]> rt_stack;
  u32 max_stack_size {10240};

  SmallVector<JSValue, 12> func_params;
  u32 func_param_cnt;

  
};

} // namespace njs

#endif // NJS_NJSVM_H