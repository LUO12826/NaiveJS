#ifndef NJS_NJSVM_H
#define NJS_NJSVM_H

#include <cstdint>
#include <memory>
#include <functional>

#include "njs/common/enums.h"
#include "NativeFunction.h"
#include "Instructions.h"
#include "njs/basic_types/GlobalObject.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCHeap.h"
#include "njs/include/SmallVector.h"
#include "njs/common/StringPool.h"
#include "JSRunLoop.h"

namespace njs {

using u32 = uint32_t;
using std::u16string;
using llvm::SmallVector;

class CodegenVisitor;
struct JSTask;

class NjsVM {
friend class JSRunLoop;
friend class GCHeap;
friend class JSObject;
friend class InternalFunctions;

 public:
  // These parameters are only for temporary convenience
  explicit NjsVM(CodegenVisitor& visitor);

  void add_native_func_impl(u16string name, NativeFuncType func);
  void add_builtin_object(const u16string& name, const std::function<JSObject*(GCHeap&, StringPool&)>& builder);
  void setup();
  void run();

 private:
  void execute();
  void execute_task(JSTask& task);
  // push
  void exec_push(int scope, int index);
  void exec_push_str(int str_idx, bool atom);
  void exec_push_this();
  // pop or store
  void exec_pop(int scope, int index);
  void exec_store(int scope, int index);
  // function operation
  void exec_make_func(int meta_idx);
  void exec_capture(int scope, int index);
  void exec_call(int arg_count, bool has_this_object);
  void exec_return();
  // object operation
  void exec_make_object();
  void exec_add_props(int props_cnt);
  void exec_keypath_access(int key_cnt, bool get_ref);
  void exec_index_access(bool get_ref);
  void exec_prop_assign();
  void exec_compound_assign(InstType type, int opr1, int opr2);
  // array operation
  void exec_make_array(int length);
  void exec_add_elements(int elements_cnt);
  // binary operation
  void exec_fast_assign(Instruction& inst);
  void exec_fast_add(Instruction& inst);
  void exec_comparison(InstType type);

  void exec_add();
  void exec_binary(InstType op_type);
  void exec_logi(InstType op_type);
  void exec_strict_equality(bool flip);

  void exec_add_assign(int scope, int index);
  void exec_inc_or_dec(int scope, int index, int inc);

  void exec_var_dispose(int scope, int index);

  JSFunction *function_env();
  JSValue& get_value(ScopeType scope, int index);
  bool are_strings_equal(const JSValue& lhs, const JSValue& rhs);
  double to_numeric_value(JSValue& val);

  constexpr static u32 frame_meta_size {2};
  u32 max_stack_size {10240};

  // stack pointer
  JSValue *sp;
  // program counter
  u32 pc {0};
  // start of a stack frame
  JSValue *frame_base_ptr;
  JSValue *rt_stack_data_begin;
  u32 func_arg_count {0};

  GCHeap heap;
  // Now still using vector because it's good for debug
  std::vector<JSValue> rt_stack;
  std::vector<Instruction> bytecode;
  JSRunLoop runloop;

  // for constant
  StringPool str_pool;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  JSValue invoker_this {JSValue::UNDEFINED};
  JSValue top_level_this;
  JSValue global_object;
  unordered_flat_map<u16string, NativeFuncType> native_func_binding;

  std::vector<std::string> log_buffer;
};

} // namespace njs

#endif // NJS_NJSVM_H