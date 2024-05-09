#ifndef NJS_NJSVM_H
#define NJS_NJSVM_H

#include <cstdint>
#include <memory>
#include <functional>

#include "njs/common/enums.h"
#include "NativeFunction.h"
#include "Instructions.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/basic_types/JSValue.h"
#include "njs/gc/GCHeap.h"
#include "njs/include/SmallVector.h"
#include "njs/common/StringPool.h"
#include "JSRunLoop.h"
#include "Runtime.h"
#include "ErrorOr.h"

namespace njs {

using u32 = uint32_t;
using std::u16string;
using llvm::SmallVector;
using SPRef = JSValue*&;

class CodegenVisitor;
struct JSTask;

struct CallFlags {
  bool constructor : 1 {false};
  bool copy_args : 1 {false};
  bool buffer_on_heap : 1 {false};
  bool generator : 1 {false};
};

struct JSStackFrame {
  JSStackFrame *prev_frame {nullptr};
  JSValue function;
  size_t alloc_cnt {0};
  JSValue *buffer {nullptr};
  JSValue *args_buf {nullptr};
  JSValue *local_vars {nullptr};
  JSValue *stack {nullptr};
  JSValue *sp {nullptr};
  u32 pc {0};
  u32 *pc_ref {nullptr};

  JSStackFrame *move_to_heap() {
    JSStackFrame& frame = *new JSStackFrame(*this);
    frame.buffer = (JSValue*)malloc(sizeof(JSValue) * frame.alloc_cnt);
    memcpy(frame.buffer, this->buffer, sizeof(JSValue) * frame.alloc_cnt);
    auto addr_diff = frame.buffer - this->buffer;
    frame.args_buf += addr_diff;
    frame.local_vars += addr_diff;
    frame.stack += addr_diff;
    frame.sp += addr_diff;

    return &frame;
  }
};

class NjsVM {
friend struct JSValue;
friend class JSRunLoop;
friend class GCHeap;
friend class JSBoolean;
friend class JSNumber;
friend class JSString;
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
  ~NjsVM();

  void add_native_func_impl(const u16string& name,
                            NativeFuncType func,
                            const std::function<void(JSFunction&)>& builder);
  void add_builtin_object(const u16string& name,
                          const std::function<JSObject*()>& builder);
  void setup();
  void run();

  std::vector<StackTraceItem> capture_stack_trace();

  JSObject* new_object(ObjectClass cls = ObjectClass::CLS_OBJECT);
  JSFunction* new_function(const JSFunctionMeta& meta);

  u32 sv_to_atom(u16string_view str_view) {
    return str_pool.atomize_sv(str_view);
  }

  u32 str_to_atom(const u16string& str) {
    return str_pool.atomize(str);
  }

  u16string& atom_to_str(int64_t atom) {
    return str_pool.get_string(atom);
  }

 private:
  void execute();
  void execute_task(JSTask& task);
  Completion call_function(JSFunction *func, JSObject *this_obj,
                           const std::vector<JSValue>& args, CallFlags flags = CallFlags());
  Completion call_internal(JSFunction *callee, JSValue this_obj, ArrayRef<JSValue> argv, CallFlags flags);
  // function operation
  void exec_make_func(SPRef sp, int meta_idx, JSValue env_this);
  CallResult exec_call(SPRef sp, int arg_count, bool has_this_object);
  void exec_js_new(SPRef sp, int arg_count);
  // object operation
  void exec_add_props(SPRef sp, int props_cnt);
  void exec_key_access(SPRef sp, u32 key_atom, bool get_ref, int keep_obj);
  void exec_index_access(SPRef sp, bool get_ref, int keep_obj);
  void exec_set_prop_atom(SPRef sp, u32 key_atom);
  void exec_set_prop_index(SPRef sp);
  void exec_dynamic_get_var(SPRef sp, u32 name_atom);
  // array operation
  void exec_add_elements(SPRef sp, int elements_cnt);
  // binary operation
  void exec_comparison(SPRef sp, OpType type);

  void exec_add(SPRef sp);
  void exec_binary(SPRef sp, OpType op_type);
  void exec_logi(SPRef sp, OpType op_type);
  void exec_bits(SPRef sp, OpType op_type);
  void exec_shift(SPRef sp, OpType op_type);
  void exec_strict_equality(SPRef sp, bool flip);
  void exec_abstract_equality(SPRef sp, bool flip);

  void exec_halt_err(SPRef sp, Instruction &inst);

  bool key_access_on_primitive(SPRef sp, JSValue& obj, int64_t atom, int keep_obj);
  JSValue index_object(JSValue obj, JSValue index, bool get_ref);

  void error_throw(SPRef sp, const u16string& msg);
  void error_handle(SPRef sp);
  void print_unhandled_error(JSValue err_val);

  void init_prototypes();

  // currently see the global scope as a big, outermost function.
  JSFunctionMeta global_meta;

  // true if the code in the global scope is executed
  bool global_end {false};

  GCHeap heap;

  std::vector<JSStackFrame*> stack_frames;
  JSStackFrame *curr_frame {nullptr};

  std::vector<Instruction> bytecode;
  JSRunLoop runloop;

  // for constant
  StringPool str_pool;
  SmallVector<double, 10> num_list;
  SmallVector<JSFunctionMeta, 10> func_meta;

  JSValue global_object;

  // for error handling in the global scope
  SmallVector<CatchTableEntry, 3> global_catch_table;

  // for collecting all log strings.
  std::vector<std::string> log_buffer;

  // object prototypes
  JSValue object_prototype;
  JSValue array_prototype;
  JSValue number_prototype;
  JSValue boolean_prototype;
  JSValue string_prototype;
  JSValue function_prototype;
};

} // namespace njs

#endif // NJS_NJSVM_H