#ifndef NJS_NJSVM_H
#define NJS_NJSVM_H

#include <cstdint>
#include <memory>

#include "njs/common/enums.h"
#include "NativeFunction.h"
#include "Instructions.h"
#include "njs/basic_types/GlobalObject.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCHeap.h"
#include "njs/include/SmallVector.h"

namespace njs {

using u32 = uint32_t;
using std::u16string;
using llvm::SmallVector;

class CodegenVisitor;

class NjsVM {
friend class GCHeap;
friend class InternalFunctions;

 public:
  // These parameters are only for temporary convenience
  explicit NjsVM(CodegenVisitor& visitor);

  void add_native_func_impl(u16string name, NativeFuncType func);
  void run();

 private:
  void execute();
  // push
  void exec_push(Instruction& inst);
  void exec_push_str(int str_idx, bool atom);
  void exec_push_this();
  // pop or store
  void exec_pop(Instruction& inst);
  void exec_store(Instruction& inst);
  // function operation
  void exec_make_func(int meta_idx);
  void exec_capture(Instruction& inst);
  void exec_call(int arg_count, bool has_this_object);
  void exec_return();
  // object operation
  void exec_make_object();
  void exec_add_props(int props_cnt);
  void exec_keypath_access(int key_cnt, bool get_ref);
  void exec_index_access(bool get_ref);
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
  u32 calc_var_addr(ScopeType scope, int raw_index);

  std::vector<Instruction> bytecode;

  // for constant
  std::vector<u16string> str_list;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  unordered_map<u16string, NativeFuncType> native_func_binding;

  JSObject *invoker_this {nullptr};
  JSObject top_level_this;
  GlobalObject global_object;
  // Now still using vector because it's good for debug
  std::vector<JSValue> rt_stack;
  u32 max_stack_size{10240};

  GCHeap heap;

  std::vector<std::string> log_buffer;

  constexpr static u32 frame_meta_size = 2;

  // stack pointer
  u32 sp;
  // program counter
  u32 pc {0};
  u32 frame_base_ptr {0};
};

} // namespace njs

#endif // NJS_NJSVM_H