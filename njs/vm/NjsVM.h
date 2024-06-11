#ifndef NJS_NJSVM_H
#define NJS_NJSVM_H

#include <cstdint>
#include <memory>
#include <deque>
#include <vector>
#include <functional>

#include "JSRunLoop.h"
#include "Runtime.h"
#include "NativeFunction.h"
#include "Instruction.h"
#include "njs/gc/GCHeap.h"
#include "njs/common/enums.h"
#include "njs/common/common_def.h"
#include "njs/common/AtomPool.h"
#include "njs/basic_types/JSFunction.h"
#include "njs/basic_types/JSErrorPrototype.h"
#include "njs/basic_types/JSValue.h"
#include "njs/basic_types/REByteCode.h"
#include "njs/include/SmallVector.h"
#include "njs/include/robin_hood.h"

namespace njs {

using u32 = uint32_t;
using std::u16string;
using std::vector;
using std::deque;
using std::unique_ptr;
using llvm::SmallVector;
using SPRef = JSValue*&;
using ArgRef = ArrayRef<JSValue>;

class CodegenVisitor;
struct JSTask;

struct JSStackFrame {
  JSStackFrame *prev_frame;
  JSValue function;
  size_t alloc_cnt {0};
  JSValue *buffer;
  JSValue *args_buf;
  JSValue *local_vars;
  JSValue *stack;
  JSValue *sp;
  u32 pc {0};
  // warning: these two values are valid only when this frame is active.
  JSValue **sp_ref;
  u32 *pc_ref;

  JSStackFrame* move_to_heap() {
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
friend class JSDate;
friend class JSRegExp;
friend class JSForInIterator;
friend class JSObjectPrototype;
friend class JSArrayPrototype;
friend class JSFunctionPrototype;
friend class JSStringPrototype;
friend class JSErrorPrototype;
friend class NativeFunction;
friend class JSArrayIterator;

 public:

  struct StackTraceItem {
    u16string func_name;
    u32 source_line;
    bool is_native;
  };

  // These parameters are only for temporary convenience
  explicit NjsVM(CodegenVisitor& visitor);
  ~NjsVM();

  JSFunction* add_native_func_impl(u16string_view name, NativeFuncType func);
  JSObject* add_builtin_object(const u16string& name);
  void add_builtin_global_var(const u16string& name, JSValue val);

  template<JSErrorType type>
  void add_error_ctor() {
    JSFunction *func = add_native_func_impl(
        // name
        native_error_name[type],
        // function
        [] (vm_func_This_args_flags) {
          return NativeFunction::error_ctor_internal(vm, args, type);
        }
    );

    native_error_protos[type].as_object
        ->add_prop_trivial(*this, AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(*this, AtomPool::k_prototype, error_prototype);
  }

  void setup();
  void run();

  vector<StackTraceItem> capture_stack_trace();
  u16string build_trace_str(bool remove_top = false);
  JSValue build_error_internal(JSErrorType type, const u16string& msg);
  JSValue build_error_internal(JSErrorType type, u16string&& msg);

  JSValue build_cannot_access_prop_error(JSValue key, JSValue obj, bool is_set);

  JSObject* new_object(ObjClass cls = CLS_OBJECT);
  JSObject* new_object(ObjClass cls, JSValue proto);
  JSFunction* new_function(JSFunctionMeta *meta);
  JSValue new_primitive_string(const u16string& str);
  JSValue new_primitive_string(u16string_view str);
  JSValue new_primitive_string(const char16_t *str);
  JSValue new_primitive_string(char16_t str);

  bool has_atom_str(u16string_view str_view) {
    return atom_pool.has_string(str_view);
  }

  u32 str_to_atom(u16string_view str_view) {
    return atom_pool.atomize(str_view);
  }

  u32 str_to_atom_no_uint(u16string_view str_view) {
    return atom_pool.atomize_no_uint(str_view);
  }

  u32 u32_to_atom(u32 num) {
    return atom_pool.atomize_u32(num);
  }

  u32 new_symbol() {
    return atom_pool.atomize_symbol();
  }

  u32 new_symbol_desc(u16string_view desc) {
    return atom_pool.atomize_symbol_desc(desc);
  }

  /// warning: do NOT keep the returned view for long. It will be invalidated at the next call.
  u16string_view atom_to_str(u32 atom) {
    if (atom_is_int(atom)) [[unlikely]] {
      temp_int_atom_string = to_u16string(atom_get_int(atom));
      return temp_int_atom_string;
    } else {
      return atom_pool.get_string(atom);
    }
  }

  JSValue get_string_const(size_t index) {
    return string_const[index];
  }

  GCHeap heap;
  
 private:
  void execute_global();
  void execute_task(JSTask& task);
  void execute_single_task(JSTask& task);
  void execute_pending_task();

  Completion call_function(JSValueRef func, JSValueRef This, JSValueRef new_target,
                           const vector<JSValue>& args, CallFlags flags = CallFlags());
  Completion call_internal(JSValueRef callee, JSValueRef This, JSValueRef new_target,
                           ArgRef argv, CallFlags flags);
  // function operation
  void exec_make_func(SPRef sp, int meta_idx, JSValue env_this);
  void exec_js_new(SPRef sp, int arg_count);
  // object operation
  void exec_add_props(SPRef sp, int props_cnt);
  void exec_get_prop_atom(SPRef sp, u32 key_atom, int keep_obj);
  void exec_get_prop_index(SPRef sp, int keep_obj);
  void exec_set_prop_atom(SPRef sp, u32 key_atom);
  void exec_set_prop_index(SPRef sp);
  void exec_dynamic_get_var(SPRef sp, u32 name_atom, bool no_throw);
  void exec_dynamic_set_var(SPRef sp, u32 name_atom);
  // array operation
  void exec_add_elements(SPRef sp, int elements_cnt);
  // binary operation
  void exec_comparison(SPRef sp, OpType type);

  void exec_add_common(SPRef sp, JSValue& dest, JSValue& lhs, JSValue& rhs, bool& succeeded);
  void exec_add_assign(SPRef sp, JSValue& target, bool keep_value);
  void exec_binary(SPRef sp, OpType op_type);
  void exec_bits(SPRef sp, OpType op_type);
  void exec_shift(SPRef sp, OpType op_type);
  void exec_shift_imm(SPRef sp, OpType op_type, u32 imm);
  void exec_abstract_equality(SPRef sp, bool flip);

  JSValue exec_typeof(JSValue val);
  void exec_in(SPRef sp);
  void exec_instanceof(SPRef sp);
  void exec_delete(SPRef sp);

  void exec_regexp_build(SPRef sp, u32 atom, int reflags);

  void exec_halt_err(SPRef sp, Instruction &inst);

  Completion get_prop_on_primitive(JSValue& obj, JSValue key);
  Completion get_prop_common(JSValue obj, JSValue key);
  Completion set_prop_common(JSValue obj, JSValue key, JSValue value);

  Completion for_of_get_iterator(JSValue obj);
  Completion for_of_call_next(JSValue iter);

  void error_throw(SPRef sp, const u16string& msg);
  void error_throw(SPRef sp, JSErrorType type, const u16string& msg);
  void error_throw_handle(SPRef sp, JSErrorType type, const u16string& msg);
  void error_handle(SPRef sp);
  void print_unhandled_error(JSValue err);

  JSValue prepare_arguments_array(ArgRef args);

  void init_prototypes();
  void show_stats();

  // currently see the global scope as a big, outermost function.
  JSFunctionMeta global_meta;

  // true if the code in the global scope is executed
  bool global_end {false};

  JSStackFrame *curr_frame {nullptr};
  JSStackFrame *global_frame {nullptr};

  vector<Instruction> bytecode;
  JSRunLoop runloop;
  deque<JSTask> micro_task_queue;

  // for constant
  AtomPool atom_pool;
  SmallVector<double, 10> num_list;
  vector<unique_ptr<JSFunctionMeta>> func_meta;

  JSValue global_object;
  JSValue global_func;

  // for collecting all log strings.
  vector<std::string> log_buffer;

  // object prototypes
  JSValue object_prototype;
  JSValue array_prototype;
  JSValue number_prototype;
  JSValue boolean_prototype;
  JSValue string_prototype;
  JSValue function_prototype;
  JSValue error_prototype;
  JSValue regexp_prototype;
  JSValue date_prototype;
  JSValue iterator_prototype;
  vector<JSValue> native_error_protos;

  vector<JSValue> string_const;
  unordered_flat_map<u32, REByteCode> regexp_bytecode;
  u16string temp_int_atom_string;

  vector<int> make_function_counter;
};

} // namespace njs

#endif // NJS_NJSVM_H