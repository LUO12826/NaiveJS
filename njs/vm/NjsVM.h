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
#include "Runtime.h"

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
friend class JSFunction;
friend class JSArray;
friend class JSObjectPrototype;
friend class JSArrayPrototype;
friend class JSFunctionPrototype;
friend class JSStringPrototype;
friend class InternalFunctions;

 public:

  struct StackTraceItem {
    u16string func_name;
    u32 source_line;
    bool is_native;
  };

  // These parameters are only for temporary convenience
  explicit NjsVM(CodegenVisitor& visitor);

  void add_native_func_impl(u16string name, NativeFuncType func);
  void add_builtin_object(const u16string& name, const std::function<JSObject*(GCHeap&, StringPool&)>& builder);
  void setup();
  void run();

  void pop_drop();
  JSValue peek_stack_top();
  void push_stack(JSValue val);

  std::vector<StackTraceItem> capture_stack_trace();

  JSObject* new_object(ObjectClass cls, JSValue prototype);
  JSObject* new_object(ObjectClass cls = ObjectClass::CLS_OBJECT);
  JSArray* new_array(int length);
  JSFunction* new_function(const JSFunctionMeta& meta);

 private:
  int execute(bool stop_at_return = false);
  void execute_task(JSTask& task);
  int call_function(JSFunction *func, const std::vector<JSValue>& args, JSObject *this_obj);
  void prepare_for_call(JSFunction *func, const std::vector<JSValue>& args, JSObject *this_obj);
  // push
  void exec_push(int scope, int index);
  void exec_push_str(int str_idx, bool atom);
  void exec_push_this(bool in_global);
  // pop or store
  void exec_pop(int scope, int index);
  void exec_store(int scope, int index);
  // function operation
  void exec_make_func(int meta_idx);
  void exec_capture(int scope, int index);
  CallResult exec_call(int arg_count, bool has_this_object);
  void exec_js_new(int arg_count);
  void exec_return();
  void exec_return_error();
  // object operation
  void exec_make_object();
  void exec_add_props(int props_cnt);
  void exec_key_access(int key_atom, bool get_ref);
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

  void exec_var_deinit_range(int start, int end);
  void exec_var_dispose(int scope, int index);
  void exec_var_dispose_range(int start, int end);
  void exec_halt_err(Instruction &inst);

  JSFunction *function_env();
  JSValue& get_value(ScopeType scope, int index);
  bool are_strings_equal(const JSValue& lhs, const JSValue& rhs);

  bool key_access_on_primitive(JSValue& obj, int64_t atom);

  void error_throw(const u16string& msg);
  void error_handle();

  void init_prototypes();

  constexpr static u32 frame_meta_size {2};
  u32 max_stack_size {10240};

  // stack pointer
  JSValue *sp;
  JSValue *global_sp;
  // program counter
  u32 pc {0};
  // start of a stack frame
  JSValue *frame_base_ptr;
  JSValue *rt_stack_begin;
  u32 func_arg_count {0};

  // true if the code in the global scope is executed
  bool global_end {false};

  GCHeap heap;
  std::vector<JSValue> rt_stack;
  std::vector<Instruction> bytecode;
  JSRunLoop runloop;

  // for constant
  StringPool str_pool;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  JSValue invoker_this;
  JSValue top_level_this;
  JSValue global_object;

  // for error handling in the global scope
  SmallVector<CatchTableEntry, 3> global_catch_table;
  // native functions and pointers to them
  unordered_flat_map<u16string, NativeFuncType> native_func_binding;
  // for collecting all log strings.
  std::vector<std::string> log_buffer;

  // object prototypes
  JSValue object_prototype;
  JSValue array_prototype;
  JSValue string_prototype;
  JSValue function_prototype;
};

} // namespace njs

#endif // NJS_NJSVM_H