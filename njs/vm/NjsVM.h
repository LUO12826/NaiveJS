#ifndef NJS_NJSVM_H
#define NJS_NJSVM_H

#include <cstdint>
#include <memory>

#include "Instructions.h"
#include "njs/basic_types/GlobalObject.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/basic_types/JSValue.h"
#include "njs/include/SmallVector.h"

namespace njs {

using u32 = uint32_t;
using llvm::SmallVector;

class NjsVM {
 public:
  // These parameters are only for temporary convenience
  NjsVM(SmallVector<Instruction, 10>&& bytecode, unordered_map<u16string, u32>&& global_props_map)
      : global_object(global_props_map) {
    rt_stack = std::make_unique<JSValue[]>(max_stack_size);
    func_params.reserve(20);
    operation_stack.reserve(200);
  }

 private:
  void execute();

  SmallVector<Instruction, 10> bytecode;

  std::unique_ptr<JSValue[]> rt_stack;
  u32 max_stack_size{10240};

  SmallVector<JSValue, 200> operation_stack;

  SmallVector<JSValue, 20> func_params;
  u32 func_param_cnt;

  // stack pointer
  u32 sp;
  // program counter
  u32 pc;
  u32 frame_bottom_pointer;

  GlobalObject global_object;
};

} // namespace njs

#endif // NJS_NJSVM_H