#ifndef NJS_NJSVM_H
#define NJS_NJSVM_H

#include <cstdint>
#include <memory>

#include "njs/common/enums.h"
#include "Instructions.h"
#include "njs/basic_types/GlobalObject.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCHeap.h"
#include "njs/include/SmallVector.h"

namespace njs {

using u32 = uint32_t;
using llvm::SmallVector;

class CodegenVisitor;

class NjsVM {
friend class GCHeap;

 public:
  // These parameters are only for temporary convenience
  NjsVM(CodegenVisitor& visitor);

  void run();

 private:

  u32 calc_var_address(ScopeType scope, int raw_index);

  void execute();

  void exec_push(Instruction& inst);
  void exec_pop(Instruction& inst, bool assign);
  void exec_store(Instruction& instruction, bool assign);
  void exec_make_func(int meta_idx);
  void exec_call(int arg_count);
  void exec_make_object();
  void exec_make_array();
  void exec_fast_assign(Instruction& inst);
  void exec_add_props(int props_cnt);
  void exec_push_str(int str_idx, bool atom);
  void exec_keypath_visit(int key_cnt, bool get_ref);

  void exec_fast_add(Instruction& inst);
  void exec_return();

  SmallVector<Instruction, 10> bytecode;

  // for constant
  SmallVector<u16string, 10> str_list;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  GlobalObject global_object;

  std::unique_ptr<JSValue[]> rt_stack;
  u32 max_stack_size{10240};

  GCHeap heap;

  constexpr static u32 frame_meta_size = 2;

  // stack pointer
  u32 sp;
  // program counter
  u32 pc {0};
  u32 frame_bottom_pointer {0};
  void exec_prop_assign();
  void exec_add_elements(int elements_cnt);
};

} // namespace njs

#endif // NJS_NJSVM_H