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

  void add_native_func_impl(u16string name, NativeFuncType func);

  void run();

 private:

  u32 calc_var_address(ScopeType scope, int raw_index);

  void execute();
  // push
  void exec_push(Instruction& inst);
  void exec_push_str(int str_idx, bool atom);
  // pop or store
  void exec_pop(Instruction& inst);
  void exec_store(Instruction& inst);
  // function operation
  void exec_make_func(int meta_idx);
  void exec_capture(Instruction& inst);
  void exec_call(int arg_count);
  void exec_return();
  // object operation
  void exec_make_object();
  void exec_add_props(int props_cnt);
  void exec_keypath_visit(int key_cnt, bool get_ref);
  void exec_prop_assign();
  // array operation
  void exec_make_array();
  void exec_add_elements(int elements_cnt);
  // binary operation
  void exec_fast_assign(Instruction& inst);
  void exec_fast_add(Instruction& inst);

  void exec_add();
  void exec_binary(InstType op_type);
  void exec_logi(InstType op_type);

  JSFunction *function_env();

  std::vector<Instruction> bytecode;

  // for constant
  SmallVector<u16string, 10> str_list;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  unordered_map<u16string, NativeFuncType> native_func_binding;

  GlobalObject global_object;

  std::vector<JSValue> rt_stack;
  u32 max_stack_size{10240};

  GCHeap heap;

  constexpr static u32 frame_meta_size = 2;

  // stack pointer
  u32 sp;
  // program counter
  u32 pc {0};
  u32 frame_base_ptr {0};
};

} // namespace njs

#endif // NJS_NJSVM_H