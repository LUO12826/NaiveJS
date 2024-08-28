#include "NjsVM.h"

#include <iostream>
#include <random>
#include "JSStackFrame.h"
#include "njs/common/Completion.h"
#include "njs/basic_types/JSValue.h"
#include "njs/utils/helper.h"
#include "njs/include/libregexp/cutils.h"
#include "njs/global_var.h"
#include "njs/common/common_def.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/basic_types/JSPromise.h"
#include "njs/basic_types/JSGenerator.h"
#include "njs/basic_types/JSHeapValue.h"
#include "njs/basic_types/JSBoundFunction.h"
#include "njs/basic_types/PrimitiveString.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/testing_and_comparison.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSRegExp.h"
#include "njs/basic_types/JSForInIterator.h"

/// try something that produces `Completion`
#define VM_TRY_COMP(expression)                                                               \
    ({                                                                                        \
        Completion _temp_result = (expression);                                               \
        if (_temp_result.is_throw()) [[unlikely]] {                                           \
          *(++sp) = _temp_result.get_value();                                                 \
          error_handle(sp);                                                                   \
          return;                                                                             \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

/// try something that produces `ErrorOr<>`
#define VM_TRY_ERR(expression)                                                                \
    ({                                                                                        \
        auto _temp_result = (expression);                                                     \
        if (_temp_result.is_error()) [[unlikely]] {                                           \
          *(++sp) = _temp_result.get_error();                                                 \
          error_handle(sp);                                                                   \
          return;                                                                             \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

#define vm_write_barrier(obj, field) heap.write_barrier(obj, field)

// required by libregexp
BOOL lre_check_stack_overflow(void *opaque, size_t alloca_size) {
  return FALSE;
}
// required by libregexp
void *lre_realloc(void *opaque, void *ptr, size_t size) {
  return realloc(ptr, size);
}

namespace njs {

static size_t inst_counter[static_cast<int>(OpType::opcode_count)];

JSValue prepare_arguments_array(NjsVM& vm, ArgRef args) {
  auto *arr = vm.heap.new_object<JSArray>(vm, args.size());

  if (vm.heap.object_in_newgen(arr)) {
    for (int i = 0; i < args.size(); i++) {
      arr->get_dense_array()[i] = args[i];
    }
  } else {
    for (int i = 0; i < args.size(); i++) {
      arr->set_element_fast(vm, i, args[i]);
    }
  }
  return JSValue(arr);
}

NjsVM::NjsVM(CodegenVisitor& visitor)
  : heap(1600, *this)
  , bytecode(std::move(visitor.bytecode))
  , runloop(*this)
  , atom_pool(std::move(visitor.atom_pool))
  , num_list(std::move(visitor.num_list))
  , func_meta(std::move(visitor.func_meta))
  , random_engine(std::random_device{}())
{
  init_prototypes();
  JSObject *global_obj = new_object();
  global_object.set_val(global_obj);

  Scope& global_scope = *visitor.scope_chain[0];
  auto& global_sym_table = global_scope.get_symbol_table();

  for (const auto& [sym_name, sym_rec] : global_sym_table) {
    if (sym_rec.var_kind == VarKind::VAR || sym_rec.var_kind == VarKind::FUNCTION) {
      global_obj->set_prop(*this, sym_name, undefined);
    }
  }

  global_meta.name_index = atom_pool.atomize(u"(global)");
  global_meta.is_strict = global_scope.is_strict;
  global_meta.local_var_count = global_scope.get_var_count();
  global_meta.stack_size = global_scope.get_max_stack_size() + 1;
  global_meta.param_count = 0;
  global_meta.bytecode_start = 0;
  global_meta.bytecode_end = bytecode.size();
  global_meta.source_line = 0;
  global_meta.catch_table = std::move(global_scope.catch_table);

  atom_pool.record_static_atom_count();
  make_function_counter.resize(func_meta.size());
}

NjsVM::~NjsVM() {
  assert(global_frame);
  free(global_frame);
}


JSObject* NjsVM::new_object(ObjClass cls) {
  return heap.new_object<JSObject>(*this, cls, object_prototype);
}

JSObject* NjsVM::new_object(ObjClass cls, JSValue proto) {
  return heap.new_object<JSObject>(*this, cls, proto);
}

JSFunction* NjsVM::new_function(JSFunctionMeta *meta) {
  JSFunction *func;
  if (meta->is_anonymous) {
    func = heap.new_object<JSFunction>(*this, meta);
  } else {
    func = heap.new_object<JSFunction>(*this, atom_to_str(meta->name_index), meta);
  }
  int obj_class = 0;
  obj_class |= meta->is_async;
  obj_class |= meta->is_generator << 1;

  if (obj_class != 0) [[unlikely]] {
    func->set_class((ObjClass)obj_class);
  }

  size_t captured_cnt = meta->capture_list.size();
  if (captured_cnt > 0) {
    auto *arr = heap.new_array(captured_cnt);
    func->captured_var.set_val(arr);
    for (size_t i = 0; i < captured_cnt; i++) {
      (*arr)[i].set_undefined();
    }
  }
  return func;
}

JSValue NjsVM::new_primitive_string(const u16string& str) {
  return JSValue(heap.new_prim_string(str.data(), str.size()));
}

JSValue NjsVM::new_primitive_string(u16string_view str) {
  return JSValue(heap.new_prim_string(str.data(), str.size()));
}

JSValue NjsVM::new_primitive_string(const char16_t *str) {
  return JSValue(heap.new_prim_string(str, std::char_traits<char16_t>::length(str)));
}

JSValue NjsVM::new_primitive_string(char16_t ch) {
  return JSValue(heap.new_prim_string(&ch, 1));
}

JSValue NjsVM::new_primitive_string_ref(u16string_view str) {
  return JSValue(heap.new_prim_string_ref(str));
}

void NjsVM::run() {
  memset(inst_counter, 0, sizeof(inst_counter));
  execute_global();
  execute_pending_task();
  runloop.loop();

  if (Global::show_log_buffer && !log_buffer.empty()) {
    std::cout << "------------------------------" << '\n';
    std::cout << "log:" << '\n';
    for (auto& str: log_buffer) {
      std::cout << str;
    }
  }

  show_stats();
}

void NjsVM::execute_global() {
  global_func.set_val(heap.new_object<JSFunction>(*this, &global_meta));
  call_internal(global_func, global_object, undefined, ArgRef(nullptr, 0), CallFlags());
}

Completion NjsVM::call_internal(JSValueRef callee, JSValueRef This, JSValueRef new_target,
                                ArgRef argv, CallFlags flags, ResumableFuncState *state) {

  static void *dispatch_table[static_cast<int>(OpType::opcode_count) + 1] = {
#define DEF(opc) &&case_op_##opc,
#include "opcode.h"
#undef DEF
      &&case_default,
  };

#define Switch(pc) {                                                                      \
  inst = bytecode[(pc)++];                                                                \
  int op_index = static_cast<int>(inst.op_type);                                          \
  goto *dispatch_table[op_index];                                                         \
}

#define Case(opc) case_op_##opc
#define Default case_default
  //#define Break {                                                                           \
//  if (Global::show_vm_exec_steps) [[unlikely]] show_step(inst);                           \
//  inst = bytecode[(pc)++];                                                                \
//  int op_index = static_cast<int>(inst.op_type);                                          \
//  inst_counter[op_index] += 1;                                                            \
//  goto *dispatch_table[op_index];                                                         \
//}

#define Break {                                                                           \
  inst = bytecode[(pc)++];                                                                \
  int op_index = static_cast<int>(inst.op_type);                                          \
  goto *dispatch_table[op_index];                                                         \
}

#define this_func (callee.as_func)
#define get_scope (scope_type_from_int(inst.operand.two[0]))
#define opr1 (inst.operand.two[0])
#define opr2 (inst.operand.two[1])

  assert(callee.is_function());
  // Use this with caution. Can only be used in situations where GC will not occur.
  JSFunction *function = callee.as_func;

  if (state == nullptr) [[likely]] {
    switch (function->get_class()) {
      case CLS_ASYNC_FUNC:
        return async_initial_call(callee, This, argv, flags);
      case CLS_GENERATOR_FUNC:
        return generator_initial_call(callee, This, argv, flags);
      case CLS_ASYNC_GENERATOR_FUNC:
        assert(false);
        break;
      case CLS_BOUND_FUNCTION:
        assert(false);
      default:
        break;
    }
  }

  if (Global::show_vm_exec_steps) {
    printf("*** call function: %s\n", to_u8string(this_func->name).c_str());
  }

  // setup call stack
  JSStackFrame _frame;
  JSStackFrame& frame = likely(state == nullptr) ? _frame : state->stack_frame;
  frame.prev_frame = curr_frame;
  frame.function = callee;
  curr_frame = &frame;

  if (function->is_native()) {
    bool has_new_target = new_target.is_object();
    flags.this_is_new_target = has_new_target;
    JSValueRef this_arg = unlikely(has_new_target) ? new_target : This;
    Completion comp = function->native_func(*this, callee, this_arg, argv, flags);

    curr_frame = frame.prev_frame;
    return comp;
  }

  JSValue *buffer;
  JSValue *args_buf;
  JSValue *local_vars;
  JSValue *stack;
  JSValue *sp;
  u32 pc;

  frame.sp_ref = &sp;
  frame.pc_ref = &pc;

  if (state == nullptr) [[likely]] {
    bool copy_argv = (argv.size() < function->param_count) | flags.copy_args;

    size_t actual_arg_cnt = std::max(argv.size(), (size_t)function->param_count);
    size_t args_buf_cnt = unlikely(copy_argv) ? actual_arg_cnt : 0;
    size_t alloc_cnt =
        args_buf_cnt + frame_meta_size + function->local_var_count + function->stack_size;

    buffer = (JSValue *)alloca(sizeof(JSValue) * alloc_cnt);
    args_buf = unlikely(copy_argv) ? buffer : argv.data();
    local_vars = buffer + args_buf_cnt;
    stack = local_vars + function->local_var_count + frame_meta_size;

    // Initialize arguments
    if (copy_argv) [[unlikely]] {
      memcpy(args_buf, argv.data(), sizeof(JSValue) * argv.size());
      for (JSValue *val = args_buf + argv.size(); val < args_buf + args_buf_cnt; val++) {
        val->set_undefined();
      }
    }

    for (JSValue *val = args_buf; val < args_buf + argv.size(); val++) {
      set_referenced(*val);
    }

    // Initialize local variables and operation stack to undefined
    for (JSValue *val = local_vars; val < stack; val++) {
      val->set_undefined();
    }

    if (function->need_arguments_array) [[unlikely]] {
      local_vars[0] = prepare_arguments_array(*this, argv);
    }

    frame.alloc_cnt = alloc_cnt;
    frame.buffer = buffer;
    frame.args_buf = args_buf;
    frame.local_vars = local_vars;
    frame.stack = stack;
    sp = stack - 1;
    pc = this_func->bytecode_start;
  }
  else {
    state->active = true;

    buffer = frame.buffer;
    args_buf = frame.args_buf;
    local_vars = frame.local_vars;
    stack = frame.stack;
    sp = frame.sp;
    pc = frame.pc;

    if (state->resume_with_throw) {
      state->resume_with_throw = false;
      error_handle(sp);
    }
  }

  auto get_value = [&, this](ScopeType scope, int index) -> JSValue& {
    switch (scope) {
      case ScopeType::GLOBAL: {
        JSValue& val = global_frame->local_vars[index];
        return likely(val.tag != JSValue::HEAP_VAL) ? val : val.as_heap_val->wrapped_val;
      }
      case ScopeType::FUNC: {
        JSValue& val = local_vars[index];
        return likely(val.tag != JSValue::HEAP_VAL) ? val : val.as_heap_val->wrapped_val;
      }
      case ScopeType::FUNC_PARAM: {
        JSValue& val = args_buf[index];
        return likely(val.tag != JSValue::HEAP_VAL) ? val : val.as_heap_val->wrapped_val;
      }
      case ScopeType::CLOSURE:
        return this_func->get_captured_var()[index].as_heap_val->wrapped_val;
      default:
        __builtin_unreachable();
    }
  };

  auto show_step = [&, this](Instruction& inst) {
    if (inst.op_type != OpType::call) {
      auto desc = inst.description();
      if (inst.op_type == OpType::get_prop_atom || inst.op_type == OpType::get_prop_atom2) {
        desc += " (" + to_u8string(atom_to_str(inst.operand.two[0])) + ")";
      }
      printf("%-50s sp: %-3ld   pc: %-3u\n", desc.c_str(), (sp - (stack - 1)), pc);
    }
  };

#define check_uninit {                                                                             \
  if (sp[0].is_uninited()) [[unlikely]] {                                                          \
    sp -= 1;                                                                                       \
    error_throw_handle(sp, JS_REFERENCE_ERROR, u"Cannot access a variable before initialization"); \
  }                                                                                                \
}

#define pop_check_uninit {                                                                        \
  sp -= 1;                                                                                        \
  if (val.is_uninited()) [[unlikely]] {                                                           \
    error_throw_handle(sp, JS_REFERENCE_ERROR,                                                    \
                       u"Cannot access a variable before initialization");                        \
  } else {                                                                                        \
    set_referenced(sp[1]);                                                                        \
    val.assign(sp[1]);                                                                            \
  }                                                                                               \
}

#define deref_heap_if_needed \
  *++sp = (unlikely(val.tag == JSValue::HEAP_VAL) ? val.as_heap_val->wrapped_val : val);

  while (true) {
    Instruction inst;

    Switch (pc) {
      Case(init):
        global_frame = curr_frame;
        Break;
      Case(neg):
        assert(sp[0].is_float64());
        sp[0].as_f64 = -sp[0].as_f64;
        Break;
      Case(add): {
        sp -= 1;
        JSValue& l = sp[0];
        JSValue& r = sp[1];

        if (l.is_float64() && r.is_float64()) {
          l.as_f64 += r.as_f64;
        }
        else if (l.is_prim_string() && r.is_prim_string()) {
          auto *res = l.as_prim_string->concat(heap, r.as_prim_string);
          l.set_val(res);
        }
        else {
          bool succeeded;
          exec_add_common(sp, l, l, r, succeeded);
        }
        Break;
      }
      Case(sub):
      Case(mul):
      Case(div):
      Case(mod):
        exec_binary(sp, inst.op_type);
        Break;
      Case(inc): {
        JSValue& value = get_value(get_scope, opr2);
        assert(value.is_float64());
        value.as_f64 += 1;
        Break;
      }
      Case(dec): {
        JSValue& value = get_value(get_scope, opr2);
        assert(value.is_float64());
        value.as_f64 -= 1;
        Break;
      }
      Case(add_assign):
      Case(add_assign_keep):
        exec_add_assign(sp, get_value(get_scope, opr2), inst.op_type == OpType::add_assign_keep);
        Break;
      Case(add_to_left): {
        sp -= 1;
        JSValue& l = sp[0];
        JSValue& r = sp[1];
        if (l.is_float64() && r.is_float64()) {
          l.as_f64 += r.as_f64;
        } else if (l.is_prim_string() && r.is_prim_string()) {
          PrimitiveString *res;
          if (l.as_GCObject->get_ref_count() <= 1) {
            // use append mode
            res = l.as_prim_string->append(heap, r.as_prim_string);
          } else {
            res = l.as_prim_string->concat(heap, r.as_prim_string);
          }
          l.set_val(res);
        } else {
          bool succeeded;
          exec_add_common(sp, l, l, r, succeeded);
        }
        Break;
      }
      Case(logi_and):
        sp -= 1;
        if (sp[0].bool_value()) {
          sp[0] = sp[1];
        }
        Break;
      Case(logi_or):
        sp -= 1;
        if (sp[0].is_falsy()) {
          sp[0] = sp[1];
        }
        Break;
      Case(logi_not): {
        bool bool_val = sp[0].bool_value();
        sp[0].set_bool(!bool_val);
        Break;
      }
      Case(bits_and):
      Case(bits_or):
      Case(bits_xor):
        exec_bits(sp, inst.op_type);
        Break;
      Case(bits_not): {
        auto res = js_to_int32(*this, sp[0]);
        if (res.is_value()) {
          int v = ~res.get_value();
          sp[0].set_float(v);
        } else {
          sp[0] = res.get_error();
          error_handle(sp);
        }
        Break;
      }

      Case(lsh):
      Case(rsh):
      Case(ursh):
        exec_shift(sp, inst.op_type);
        Break;
      Case(lshi):
      Case(rshi):
      Case(urshi):
        exec_shift_imm(sp, inst.op_type, opr1);
        Break;

      Case(push_local_noderef):
        *++sp = local_vars[opr1];
        Break;
      Case(push_local_noderef_check):
        *++sp = local_vars[opr1];
        check_uninit
        Break;
      Case(push_local): {
        JSValue val = local_vars[opr1];
        deref_heap_if_needed
        Break;
      }
      Case(push_local_check): {
        JSValue val = local_vars[opr1];
        deref_heap_if_needed
        check_uninit
        Break;
      }
      Case(push_global): {
        JSValue val = global_frame->local_vars[opr1];
        deref_heap_if_needed
        Break;
      }
      Case(push_global_check): {
        JSValue val = global_frame->local_vars[opr1];
        deref_heap_if_needed
        check_uninit
        Break;
      }
      Case(push_arg): {
        JSValue val = args_buf[opr1];
        deref_heap_if_needed
        Break;
      }
      Case(push_arg_check): {
        JSValue val = args_buf[opr1];
        deref_heap_if_needed
        check_uninit
        Break;
      }
      Case(push_closure):
        *++sp = this_func->get_captured_var()[opr1].as_heap_val->wrapped_val;
        Break;
      Case(push_closure_check):
        *++sp = this_func->get_captured_var()[opr1].as_heap_val->wrapped_val;
        check_uninit
        Break;
      Case(push_i32):
        sp += 1;
        sp[0].tag = JSValue::NUM_INT32;
        sp[0].as_i32 = opr1;
        Break;
      Case(push_f64):
        sp += 1;
        sp[0].set_float(inst.operand.num_float);
        Break;
      Case(push_str):
        sp += 1;
        if (atom_is_str_sym(opr1)) [[likely]] {
          sp[0].set_val(heap.new_prim_string_ref(atom_pool.get_string(opr1)));
        } else {
          sp[0] = new_primitive_string(atom_to_str(opr1));
        }
        Break;
      Case(push_atom):
        sp += 1;
        sp[0].tag = JSValue::JS_ATOM;
        sp[0].as_atom = opr1;
        Break;
      Case(push_bool):
        (*++sp).set_bool(opr1);
        Break;
      Case(push_func_this):
        *++sp = This;
        Break;
      Case(push_global_this):
        *++sp = global_object;
        Break;
      Case(push_null):
        (*++sp).tag = JSValue::JS_NULL;
        Break;
      Case(push_undef):
        (*++sp).tag = JSValue::UNDEFINED;
        Break;
      Case(push_uninit):
        (*++sp).tag = JSValue::UNINIT;
        Break;

      Case(pop_local):
      Case(pop_local_check):
      Case(pop_global):
      Case(pop_global_check):
      Case(pop_arg):
      Case(pop_arg_check):
      Case(pop_closure):
      Case(pop_closure_check):
        // not implemented
        assert(false);
      Case(pop): {
        set_referenced(sp[0]);
        get_value(get_scope, opr2).assign(sp[0]);
        sp -= 1;
        Break;
      }
      Case(pop_check): {
        JSValue& val = get_value(get_scope, opr2);
        sp -= 1;
        if (val.is_uninited()) [[unlikely]] {
          error_throw_handle(sp, JS_REFERENCE_ERROR,
                             u"Cannot access a variable before initialization");
        } else {
          set_referenced(sp[1]);
          val.assign(sp[1]);
        }
        Break;
      }
      Case(pop_drop):
        sp -= 1;
        Break;
      Case(store): {
        set_referenced(sp[0]);
        get_value(get_scope, opr2).assign(sp[0]);
        Break;
      }
      Case(store_check): {
        JSValue &val = get_value(get_scope, opr2);
        if (val.is_uninited()) [[unlikely]] {
          error_throw_handle(sp, JS_REFERENCE_ERROR,
                             u"Cannot access a variable before initialization");
        } else {
          set_referenced(sp[0]);
          val.assign(sp[0]);
        }
        Break;
      }
      Case(store_curr_func):
        local_vars[opr1] = JSValue(callee);
        Break;
      Case(var_deinit):
        local_vars[opr1].tag = JSValue::UNINIT;
        Break;
      Case(var_deinit_range):
        for (int i = opr1; i < opr2; i++) {
          local_vars[i].tag = JSValue::UNINIT;
        }
        Break;
      Case(var_undef):
        assert(get_value(ScopeType::FUNC, opr1).tag == JSValue::UNINIT);
        get_value(ScopeType::FUNC, opr1).set_undefined();
        Break;
      Case(loop_var_renew): {
        JSValue &val = local_vars[opr1];
        if (val.is(JSValue::HEAP_VAL)) {
          val.move_to_stack();
        }
        Break;
      }
      Case(var_dispose): {
        int index = opr1;
        local_vars[index].set_undefined();
        Break;
      }
      Case(var_dispose_range):
        for (int i = opr1; i < opr2; i++) {
          local_vars[i].set_undefined();
        }
        Break;
      Case(jmp):
        pc = opr1;
        Break;
      Case(jmp_true):
        if (sp[0].bool_value()) {
          pc = opr1;
        }
        Break;
      Case(jmp_false):
        if (sp[0].is_falsy()) {
          pc = opr1;
        }
        Break;
      Case(jmp_cond):
        if (sp[0].bool_value()) {
          pc = opr1;
        } else {
          pc = opr2;
        }
        Break;
      Case(jmp_pop):
        pc = opr1;
        sp -= 1;
        Break;
      Case(jmp_true_pop):
        if (sp[0].bool_value()) {
          pc = opr1;
        }
        sp -= 1;
        Break;
      Case(jmp_false_pop):
        if (sp[0].is_falsy()) {
          pc = opr1;
        }
        sp -= 1;
        Break;
      Case(jmp_cond_pop):
        if (sp[0].bool_value()) {
          pc = opr1;
        } else {
          pc = opr2;
        }
        sp -= 1;
        Break;
      Case(gt):
      Case(lt):
      Case(ge):
      Case(le):
        exec_comparison(sp, inst.op_type);
        Break;
      Case(ne):
        exec_abstract_equality(sp, true);
        Break;
      Case(eq):
        exec_abstract_equality(sp, false);
        Break;
      Case(ne3):
      Case(eq3): {
        sp -= 1;
        ErrorOr<bool> res = strict_equals(*this, sp[0], sp[1]);
        bool flip = inst.op_type == OpType::ne3;

        if (res.is_value()) {
          sp[0].set_bool(flip ^ res.get_value());
        } else {
          sp[0] = res.get_error();
          error_handle(sp);
        }
        Break;
      }
      Case(call): {
        if (Global::show_vm_exec_steps) {
          printf("%-50s sp: %-3ld   pc: %-3u\n", inst.description().c_str(), (sp - (stack - 1)), pc);
        }
        int argc = opr1;
        bool has_this = opr2;
        JSValue& func = sp[-argc];

        if (not func.is_function()) [[unlikely]] {
          error_throw_handle(sp, JS_TYPE_ERROR,
                             to_u16string(func.to_string(*this)) + u" is not callable");
          Break;
        }

        ArgRef call_argv(&func + 1, argc);
        Completion comp;
        if (func.as_object->is_direct_function()) [[likely]] {
          JSValue *invoker;
          if (func.as_func->is_arrow_func) [[unlikely]] {
            invoker = &func.as_func->this_or_auxiliary_data;
          } else {
            invoker = has_this ? &sp[-argc - 1] : &global_object;
          }
          comp = call_internal(func, *invoker, undefined, call_argv, CallFlags());
        }
        else {
          assert(func.as_object->get_class() == CLS_BOUND_FUNCTION);
          comp = func.as_Object<JSBoundFunction>()->call(
              *this, undefined, undefined, call_argv, CallFlags());
        }

        sp -= (argc + int(has_this));
        sp[0] = comp.get_value();

        heap.gc_if_needed();
        if (comp.is_throw()) [[unlikely]] {
          error_handle(sp);
        }
        Break;
      }
      Case(proc_call):
        sp[1].tag = JSValue::PROC_META;
        sp[1].flag_bits = pc;
        sp += 1;
        pc = opr1;
        Break;
      Case(proc_ret):
        assert(sp[0].is(JSValue::PROC_META));
        pc = sp[0].flag_bits;
        sp -= 1;
        Break;
      Case(js_new):
        exec_js_new(sp, opr1);
        Break;
      Case(make_func): {
        exec_make_func(sp, opr1, This);
        // capture
        int i = 0;
        for (auto& [var_scope, var_idx] : sp[0].as_func->meta->capture_list) {
          if (var_scope == ScopeType::CLOSURE) [[unlikely]] {
            JSValue& closure_val = this_func->get_captured_var()[var_idx];
            vm_write_barrier(sp[0].as_func->captured_var.as_heap_array, closure_val);
            sp[0].as_func->get_captured_var()[i] = closure_val;
          }
          else {
            JSValue* stack_val;
            if (var_scope == ScopeType::FUNC) {
              stack_val = local_vars + var_idx;
            } else if (var_scope == ScopeType::FUNC_PARAM) {
              stack_val = args_buf + var_idx;
            } else if (var_scope == ScopeType::GLOBAL) {
              stack_val = &global_frame->local_vars[var_idx];
            } else {
              __builtin_unreachable();
            }

            if (stack_val->tag != JSValue::HEAP_VAL) {
              stack_val->move_to_heap(*this);
            }
            vm_write_barrier(sp[0].as_func->captured_var.as_heap_array, *stack_val);
            sp[0].as_func->get_captured_var()[i] = *stack_val;
          }
          i += 1;
        }
        Break;
      }
      Case(make_obj):
        sp += 1;
        sp[0].set_val(new_object());
        Break;
      Case(make_array):
        sp += 1;
        sp[0].set_val(heap.new_object<JSArray>(*this, opr1));
        Break;
      Case(add_props):
        exec_add_props(sp, opr1);
        Break;
      Case(add_elements):
        exec_add_elements(sp, opr1);
        Break;
      Case(ret): {
        if (Global::show_vm_exec_steps) printf("ret\n");
        if (state) state->active = false;
        curr_frame = frame.prev_frame;
        return sp[0];
      }
      Case(ret_undef): {
        if (Global::show_vm_exec_steps) printf("ret_undef\n");
        if (state) state->active = false;
        curr_frame = frame.prev_frame;
        return undefined;
      }
      Case(ret_err): {
        if (Global::show_vm_exec_steps) printf("ret_err\n");
        if (state) state->active = false;
        curr_frame = frame.prev_frame;
        return CompThrow(sp[0]);
      }
      Case(await): {
        state->active = false;
        curr_frame = frame.prev_frame;
        frame.pc = pc;
        frame.sp = sp;
        return {Completion::Type::AWAIT, sp[0]};
      }
      Case(yield): {
        state->active = false;
        curr_frame = frame.prev_frame;
        frame.pc = pc;
        frame.sp = sp;
        return {Completion::Type::YIELD, sp[0]};
      }
      Case(halt):
        if (!global_end) {
          global_end = true;
          curr_frame = curr_frame->move_to_heap();
          global_frame = curr_frame;
          if (Global::show_vm_exec_steps) {
            printf("\033[33m%-50s pc: %-3u\033[0m\n\n", inst.description().c_str(), pc);
          }
          return undefined;
        }
        else {
          assert(false);
        }
      Case(halt_err):
        exec_halt_err(sp, inst);
        return undefined;
      Case(nop):
        Break;
      Case(get_prop_atom):
      Case(get_prop_atom2):
        exec_get_prop_atom(sp, opr1, inst.op_type == OpType::get_prop_atom2);
        Break;
      Case(get_prop_index):
      Case(get_prop_index2):
        exec_get_prop_index(sp, inst.op_type == OpType::get_prop_index2);
        Break;
      Case(set_prop_atom):
        exec_set_prop_atom(sp, opr1);
        Break;
      Case(set_prop_index):
        exec_set_prop_index(sp);
        Break;
      Case(dyn_get_var):
      Case(dyn_get_var_undef): {
        exec_dynamic_get_var(sp, opr1, inst.op_type == OpType::dyn_get_var_undef);
        Break;
      }
      Case(dyn_set_var):
        exec_dynamic_set_var(sp, opr1);
        Break;
      Case(dup_stack_top):
        sp[1] = sp[0];
        sp += 1;
        Break;
      Case(move_to_top1): {
        JSValue tmp = sp[-1];
        sp[-1] = sp[0];
        sp[0] = tmp;
        Break;
      }
      Case(move_to_top2): {
        JSValue tmp = sp[-2];
        sp[-2] = sp[-1];
        sp[-1] = sp[0];
        sp[0] = tmp;
        Break;
      }
      Case(for_in_init): {
        if (!sp[0].is_nil()) {
          sp[0] = js_to_object(*this, sp[0]).get_value();
        }
        // `build_for_object` can handle null and undefined.
        sp[0] = JSForInIterator::build_for_object(*this, sp[0]);
        Break;
      }
      Case(for_in_next): {
        assert(sp[0].is_object());
        assert(object_class(sp[0]) == CLS_FOR_IN_ITERATOR);
        
        auto *iter = sp[0].as_Object<JSForInIterator>();
        sp[1] = iter->next(*this);
        sp[2] = JSValue(sp[1].is_uninited());
        sp += 2;
        Break;
      }
      Case(for_of_init): {
        auto comp = for_of_get_iterator(sp[0]);
        sp[0] = comp.get_value();
        if (comp.is_throw()) [[unlikely]] {
          error_handle(sp);
        }
        Break;
      }
      Case(for_of_next): {
        assert(sp[0].is_object());
        auto comp = for_of_call_next(sp[0]);
        JSValue res = comp.get_value();
        if (comp.is_throw()) [[unlikely]] {
          sp[0] = res;
          goto for_of_next_err;
        } else if (!res.is_object()) [[unlikely]] {
          sp[0] = build_error(JS_TYPE_ERROR, u"iterator result is not an object.");
          goto for_of_next_err;
        } else {
          auto res_val = res.as_object->get_property_impl(*this, JSAtom(AtomPool::k_value));
          if (res_val.is_throw()) {
            sp[0] = res_val.get_value();
            goto for_of_next_err;
          }
          auto res_done = res.as_object->get_property_impl(*this, JSAtom(AtomPool::k_done));
          if (res_done.is_throw()) {
            sp[0] = res_done.get_value();
            goto for_of_next_err;
          }
          sp[1] = res_val.get_value();
          sp[2] = res_done.get_value();
          sp += 2;
        }
        Break;
        for_of_next_err:
          error_handle(sp);
        Break;
      }
      Case(iter_end_jmp):
        assert(sp[0].is_bool());
        if (sp[0].as_bool) {
          pc = opr1;
          // this instruction drops the `done` and `value` produced by the iterator
          // when the iteration ends.
          sp -= 2;
        } else {
          sp -= 1;
        }
        Break;
      Case(js_in):
        exec_in(sp);
        Break;
      Case(js_instanceof):
        exec_instanceof(sp);
        Break;
      Case(js_typeof):
        sp[0] = js_op_typeof(*this, sp[0]);
        Break;
      Case(js_delete):
        exec_delete(sp);
        Break;
      Case(js_to_number): {
        auto res = js_to_number(*this, sp[0]);
        if (res.is_error()) {
          sp[0] = res.get_error();
          error_handle(sp);
        } else {
          sp[0].set_float(res.get_value());
        }
        Break;
      }
      Case(regexp_build):
        exec_regexp_build(sp, opr1, opr2);
        Break;
      Default:
        assert(false);
    }
  }
}

Completion NjsVM::async_initial_call(JSValueRef func, JSValueRef This,
                                     ArgRef argv, CallFlags flags) {
  assert(func.as_object->is_direct_function());
  assert(func.as_func->is_async());
  ResumableFuncState *state = func.as_func->build_exec_state(*this, This, argv);
  
  auto *prom = heap.new_object<JSPromise>(*this);
  prom->exec_state = state;
  JSValue promise(prom);
  push_temp_root(promise);

  async_resume(promise, state);

  pop_temp_root();
  return promise;
}

void NjsVM::async_resume(JSValueRef promise, ResumableFuncState *state) {
  assert(promise.as_object->get_class() == CLS_PROMISE);
  auto comp = call_internal(state->stack_frame.function, state->This, undefined, {},
                            CallFlags(), state);

  if (comp.is_await()) {
    JSValue new_promise
      = JSPromise::Promise_resolve_reject(*this, undefined, comp.get_value(), false).get_value();
    assert(new_promise.as_object->get_class() == CLS_PROMISE);

    auto [fulfill, reject] = JSFunction::build_async_then_callback(*this, promise);
    new_promise.as_Object<JSPromise>()->then_internal(*this, fulfill, reject);
  }
  else if (comp.is_throw()) {
    JSPromise::settling_internal(*this, promise, JSPromise::REJECTED, comp.get_value());
    promise.as_Object<JSPromise>()->dispose_exec_state();
  }
  // async function returned
  else {
    JSPromise::settling_internal(*this, promise, JSPromise::FULFILLED, comp.get_value());
    promise.as_Object<JSPromise>()->dispose_exec_state();
  }

}

Completion NjsVM::generator_initial_call(JSValueRef func, JSValueRef This,
                                         ArgRef argv, CallFlags flags) {
  assert(func.as_object->is_direct_function());
  assert(func.as_func->is_generator());
  ResumableFuncState *state = func.as_func->build_exec_state(*this, This, argv);
  JSValue proto = TRYCC(func.as_func->get_prop(*this, AtomPool::k_prototype));

  auto *gen = heap.new_object<JSGenerator>(*this);
  gen->set_proto(*this, proto);
  gen->exec_state = state;
  return JSValue(gen);
}

Completion NjsVM::generator_resume(JSValueRef generator, ResumableFuncState *state) {

  auto build_result_object = [this] (bool done, JSValue val) {
    JSObject *res = new_object();
    res->add_prop_trivial(*this, AtomPool::k_done, JSValue(done), PFlag::VECW);
    res->add_prop_trivial(*this, AtomPool::k_value, val, PFlag::VECW);
    return JSValue(res);
  };
  assert(generator.as_object->get_class() == CLS_GENERATOR);

  if (generator.as_Object<JSGenerator>()->done) {
    return build_result_object(true, undefined);
  }
  auto comp = call_internal(state->stack_frame.function, state->This, undefined, {},
                            CallFlags(), state);
  NoGC nogc(*this);
  if (comp.is_yield()) {
    return build_result_object(false, comp.get_value());
  }
  else if (comp.is_throw()) {
    generator.as_Object<JSGenerator>()->done = true;
    generator.as_Object<JSGenerator>()->dispose_exec_state();
    return comp;
  }
  // generator function returned
  else {
    generator.as_Object<JSGenerator>()->done = true;
    generator.as_Object<JSGenerator>()->dispose_exec_state();
    return build_result_object(true, comp.get_value());
  }
}

void NjsVM::execute_task(JSTask& task) {
  execute_single_task(task);
  execute_pending_task();
}

void NjsVM::execute_single_task(JSTask& task) {
  Completion comp;
  if (task.use_native_func) {
    comp = task.native_task_func(*this, global_object, undefined, task.args, CallFlags());
  } else {
    comp = call_function(task.task_func, global_object, undefined, task.args, CallFlags());
  }

  if (comp.is_throw()) [[unlikely]] {
    print_unhandled_error(comp.get_value());
  }
}

void NjsVM::execute_pending_task() {
  while (!micro_task_queue.empty()) {
    JSTask& micro_task = micro_task_queue.front();
    if (!micro_task.canceled) execute_single_task(micro_task);
    micro_task_queue.pop_front();
  }
}

Completion NjsVM::call_function(JSValueRef func, JSValueRef This, JSValueRef new_target,
                                ArgRef argv, CallFlags flags) {
  if (func.as_object->is_direct_function()) [[likely]] {
    JSFunction& f = *func.as_func;
    const JSValue *actual_this;
    if (f.is_arrow_func) {
      actual_this = &f.this_or_auxiliary_data;
    } else {
      actual_this = This.is_undefined() ? &global_object : &This;
    }
    return call_internal(func, *actual_this, new_target, argv, flags);
  }
  else {
    assert(func.as_object->get_class() == CLS_BOUND_FUNCTION);
    auto *f = func.as_Object<JSBoundFunction>();
    return f->call(*this, This, new_target, argv, flags);
  }
}

vector<StackTraceItem> NjsVM::capture_stack_trace() {
  vector<StackTraceItem> trace;

  auto iter = curr_frame;
  while (iter != global_frame) {
    auto func = iter->function.as_func;
    trace.emplace_back(StackTraceItem {
        .func_name = func->meta->is_anonymous ? u"(anonymous)" : u16string(func->name),
        .source_line = func->meta->source_line,
        .is_native = func->meta->is_native,
    });
    iter = iter->prev_frame;
  }

  if (!global_end) {
    trace.emplace_back(StackTraceItem {
        .func_name = u"(global)",
        .source_line = 0,
        .is_native = false,
    });
  }

  return trace;
}

u16string NjsVM::build_trace_str(bool remove_top) {
  std::vector<StackTraceItem> trace = capture_stack_trace();
  u16string trace_str;
  bool first = true;

  for (auto& tr : trace) {
    if (first) {
      first = false;
      if (remove_top) continue;
    }
    trace_str += u"  ";
    trace_str += tr.func_name;
    if (not tr.is_native) {
      trace_str += u"  @ line ";
      trace_str += to_u16string(tr.source_line);
    }
    else {
      trace_str += u"  (native)";
    }
    trace_str += u"\n";
  }
  return trace_str;
}

JSValue NjsVM::build_error(JSErrorType type, u16string_view msg) {
  auto *err_obj = new_object(CLS_ERROR, native_error_protos[type]);
  err_obj->set_prop(*this, u"message", new_primitive_string(msg));

  u16string trace_str = build_trace_str();
  err_obj->set_prop(*this, u"stack", new_primitive_string(trace_str));

  return JSValue(err_obj);
}

Completion NjsVM::throw_error(JSErrorType type, u16string_view msg) {
  return CompThrow(build_error(type, msg));
}

JSValue NjsVM::build_cannot_access_prop_error(JSValue key, JSValue obj, bool is_set) {
  u16string msg = is_set ? u"cannot set property '" : u"cannot read property '";
  if (key.is_atom()) {
    msg += atom_to_str(key.as_atom);
  } else {
    Completion comp = js_to_string(*this, key);
    assert(comp.is_normal());
    msg += comp.get_value().as_prim_string->view();
  }
  msg += u"' of ";
  msg += to_u16string(obj.to_string(*this));
  return build_error(JS_TYPE_ERROR, msg);
}

void NjsVM::exec_in(SPRef sp) {
  sp -= 1;
  // sp[0] : key, sp[1] : object.
  if (not sp[1].is_object()) [[unlikely]] {
    error_throw_handle(sp, JS_TYPE_ERROR, u"rhs of `in` is not an object");
    return;
  }

  sp[0] = VM_TRY_COMP(sp[1].as_object->has_property(*this, sp[0]));
}

void NjsVM::exec_instanceof(SPRef sp) {
  sp -= 1;
  JSValue o = sp[0];
  JSValue cls = sp[1];

  if (not o.is_object()) [[unlikely]] {
    sp[0].set_bool(false);
    return;
  }
  if (not (cls.is_object() && object_class(cls) == CLS_FUNCTION)) [[unlikely]] {
    error_throw_handle(sp, JS_TYPE_ERROR, u"rhs of `instanceof` is not a function");
    return;
  }

  bool is_instance = false;
  JSValue proto = o.as_object->get_proto();
  JSValue func_prototype = VM_TRY_COMP(cls.as_object->get_prop(*this, AtomPool::k_prototype));

  if (not func_prototype.is_object()) {
    error_throw_handle(sp, JS_TYPE_ERROR, u"");
    return;
  }

  while (!is_instance) {
    if (same_value(proto, func_prototype)) {
      is_instance = true;
    } else if (proto.is_object()) {
      proto = proto.as_object->get_proto();
    } else {
      break;
    }
  }

  sp[0].set_bool(is_instance);
}

void NjsVM::exec_delete(SPRef sp) {  
  sp -= 1;
  if (sp[0].is_object()) [[likely]] {
    JSValue key = VM_TRY_COMP(js_to_property_key(*this, sp[1]));
    bool succeeded = VM_TRY_ERR(sp[0].as_object->delete_property(key));
    sp[0].set_bool(succeeded);
  } else {
    sp[0].set_bool(true);
  }
}

void NjsVM::exec_regexp_build(SPRef sp, u32 atom, int reflags) {
  sp += 1;
  Completion comp = JSRegExp::New(*this, atom, reflags);
  sp[0] = comp.get_value();
  if (comp.is_throw()) [[unlikely]] {
    error_handle(sp);
  }
}

void NjsVM::exec_add_common(SPRef sp, JSValue& dest, JSValue& l, JSValue& r, bool& succeeded) {
  succeeded = false;
  JSValue lhs = VM_TRY_COMP(js_to_primitive(*this, l));
  JSValue rhs = VM_TRY_COMP(js_to_primitive(*this, r));

  if (lhs.is_prim_string() || rhs.is_prim_string()) {
    JSValue lhs_s = VM_TRY_COMP(js_to_string(*this, lhs));
    JSValue rhs_s = VM_TRY_COMP(js_to_string(*this, rhs));
    auto *new_str = lhs_s.as_prim_string->concat(heap, rhs_s.as_prim_string);
    dest.set_val(new_str);
  } else {
    double lhs_n = VM_TRY_ERR(js_to_number(*this, lhs));
    double rhs_n = VM_TRY_ERR(js_to_number(*this, rhs));
    dest.set_float(lhs_n + rhs_n);
  }
  succeeded = true;
}

void NjsVM::exec_add_assign(SPRef sp, JSValue& target, bool keep_value) {
  sp -= 1;
  JSValue& r = sp[1];

  if (target.is_float64() && r.is_float64()) {
    target.as_f64 += r.as_f64;
  } else if (target.is_prim_string() && r.is_prim_string()) {
    PrimitiveString *res;
    if (target.as_GCObject->get_ref_count() <= 1) {
      // use append mode
      res = target.as_prim_string->append(heap, r.as_prim_string);
    } else {
      res = target.as_prim_string->concat(heap, r.as_prim_string);
    }
    target.set_val(res);
  } else {
    bool succeeded;
    exec_add_common(sp, target, target, r, succeeded);
    if (not succeeded) [[unlikely]] {
      // `exec_add_common` has handled the error. so just return.
      return;
    }
  }
  set_referenced(target);
  if (keep_value) [[unlikely]] {
    *++sp = target;
  }
}

void NjsVM::exec_binary(SPRef sp, OpType op_type) {

  auto calc = [] (OpType op_type, double l, double r) {
    switch (op_type) {
      case OpType::sub:
        return l - r;
      case OpType::mul:
        return l * r;
      case OpType::div:
        return l / r;
      case OpType::mod:
        return fmod(l, r);
      default:
        assert(false);
    }
    __builtin_unreachable();
  };
  sp -= 1;
  if (sp[0].is_float64() && sp[1].is_float64()) [[likely]] {
    sp[0].as_f64 = calc(op_type, sp[0].as_f64, sp[1].as_f64);
  }
  else {
    double lhs = VM_TRY_ERR(js_to_number(*this, sp[0]));
    double rhs = VM_TRY_ERR(js_to_number(*this, sp[1]));
    sp[0].set_float(calc(op_type, lhs, rhs));
  }
}

void NjsVM::exec_bits(SPRef sp, OpType op_type) {

  ErrorOr<int32_t> lhs, rhs;

  lhs = js_to_int32(*this, sp[-1]);
  if (lhs.is_error()) goto error;

  rhs = js_to_int32(*this, sp[0]);
  if (rhs.is_error()) goto error;

  switch (op_type) {
    case OpType::bits_and:
      sp[-1].set_float(lhs.get_value() & rhs.get_value());
      break;
    case OpType::bits_or:
      sp[-1].set_float(lhs.get_value() | rhs.get_value());
      break;
    case OpType::bits_xor:
      sp[-1].set_float(lhs.get_value() ^ rhs.get_value());
      break;
    default:
      assert(false);
  }
  sp -= 1;
  return;

error:
  if (lhs.is_error()) sp[-1] = lhs.get_error();
  else sp[-1] = rhs.get_error();
  sp -= 1;
  error_handle(sp);
}

void NjsVM::exec_shift(SPRef sp, OpType op_type) {
  ErrorOr<int32_t> lhs;
  ErrorOr<uint32_t> rhs;
  uint32_t shift_len;

  lhs = js_to_int32(*this, sp[-1]);
  if (lhs.is_error()) goto error;

  rhs = js_to_uint32(*this, sp[0]);
  if (rhs.is_error()) goto error;

  shift_len = rhs.get_value() & 0x1f;

  switch (op_type) {
    case OpType::lsh:
      sp[-1].set_float(lhs.get_value() << shift_len);
      break;
    case OpType::rsh:
      sp[-1].set_float(lhs.get_value() >> shift_len);
      break;
    case OpType::ursh:
      sp[-1].set_float(u32(lhs.get_value()) >> shift_len);
      break;
    default:
      assert(false);
  }
  sp -= 1;
  return;

error:
  if (lhs.is_error()) sp[-1] = lhs.get_error();
  else sp[-1] = rhs.get_error();
  sp -= 1;
  error_handle(sp);
}

void NjsVM::exec_shift_imm(SPRef sp, OpType op_type, u32 imm) {
  uint32_t shift_len = imm & 0x1f;
  ErrorOr<int32_t> lhs = js_to_int32(*this, sp[0]);

  if (lhs.is_error()) {
    sp[0] = lhs.get_error();
    error_handle(sp);
    return;
  }

  switch (op_type) {
    case OpType::lsh:
      sp[0].set_float(lhs.get_value() << shift_len);
      break;
    case OpType::rsh:
      sp[0].set_float(lhs.get_value() >> shift_len);
      break;
    case OpType::ursh:
      sp[0].set_float(u32(lhs.get_value()) >> shift_len);
      break;
    default:
      assert(false);
  }
}

void NjsVM::exec_make_func(SPRef sp, int meta_idx, JSValue env_this) {
//  make_function_counter[meta_idx] += 1;
  auto *meta = func_meta[meta_idx].get();
  auto *func = new_function(meta);

  // if a function is an arrow function, it captures the `this` value in the environment
  // where the function is created.
  if (func->is_arrow_func) {
    vm_write_barrier(func, env_this);
    func->this_or_auxiliary_data = env_this;
  }

  if (not func->is_arrow_func && not func->is_async()) {
    if (func->is_generator()) [[unlikely]] {
      auto *obj = new_object(CLS_GENERATOR, generator_prototype);
      func->add_prop_trivial(*this, AtomPool::k_prototype, JSValue(obj), PFlag::VCW);
      func->set_proto(*this, generator_function_ctor);
    } else {
      // make the `prototype` property a lazy property
      PFlag flag = PFlag::VCW;
      flag.lazy_kind = LAZY_PROTOTYPE;
      func->add_prop_trivial(*this, AtomPool::k_prototype, undefined, flag);
    }
  }

  assert(!meta->is_native);

  sp += 1;
  sp[0].set_val(func);
}

void NjsVM::exec_js_new(SPRef sp, int argc) {

  if (not sp[-argc].is_function()) [[unlikely]] {
    u16string msg(js_op_typeof(*this, sp[-argc]).as_prim_string->view());
    msg += u" is not callable";
    error_throw_handle(sp, JS_TYPE_ERROR, msg);
    return;
  }

  for (int i = 1; i > -argc; i--) {
    sp[i] = sp[i - 1];
  }
  sp += 1;

  JSValue& ctor = sp[-argc];
  JSValue& This = sp[-argc - 1];

  // check whether the ctor is a constructor
  if (not ctor.as_object->is_bound_function()) [[likely]] {
    assert(ctor.as_object->is_direct_function());
    if (not ctor.as_func->is_constructor) [[unlikely]] {
      error_throw_handle(sp, JS_TYPE_ERROR, u"function is not a constructor");
      return;
    }
  }

  // prepare `this` object for non-native and non-bounded functions.
  if (ctor.as_object->is_bound_function() || ctor.as_func->is_native()) [[unlikely]] {
    This.set_undefined();
  } else {
    JSValue proto = VM_TRY_COMP(ctor.as_func->get_prop(*this, AtomPool::k_prototype));
    proto = proto.is_object() ? proto : object_prototype;
    auto *this_obj = heap.new_object<JSObject>(*this, CLS_OBJECT, proto);

    This.set_val(this_obj);
  }

  ArgRef argv(&ctor + 1, argc);
  CallFlags flags = CallFlags();
  flags.constructor = true;
  Completion comp;
  if (ctor.as_object->is_direct_function()) {
    comp = call_internal(ctor, This, ctor, argv, flags);
  } else {
    assert(ctor.as_object->is_bound_function());
    comp = ctor.as_Object<JSBoundFunction>()->call(*this, This, ctor, argv, flags);
  }
  sp -= (argc + 1);

  if (comp.is_throw()) [[unlikely]] {
    This = comp.get_value();
    error_handle(sp);
  } else {
    if (comp.get_value().is_object()) [[unlikely]] {
      This = comp.get_value();
    }
  }
}

void NjsVM::exec_add_props(SPRef sp, int props_cnt) {
  JSValue& val_obj = sp[-props_cnt * 2];
  assert(val_obj.is_object());
  JSObject *object = val_obj.as_object;

  for (JSValue *key = sp - props_cnt * 2 + 1; key <= sp; key += 2) {
    assert(key[0].is_atom());
    object->add_prop_trivial(*this, key[0].as_atom, key[1], PFlag::VECW);
  }

  sp = sp - props_cnt * 2;
}

void NjsVM::exec_add_elements(SPRef sp, int elements_cnt) {
  JSValue& val_array = sp[-elements_cnt];
  assert(val_array.tag == JSValue::ARRAY);
  JSArray *array = val_array.as_array;

  u32 ele_idx = 0;
  if (heap.object_in_newgen(array)) [[likely]] {
    for (JSValue *val = sp - elements_cnt + 1; val <= sp; val++, ele_idx++) {
      array->get_dense_array()[ele_idx] = *val;
    }
  } else {
    for (JSValue *val = sp - elements_cnt + 1; val <= sp; val++, ele_idx++) {
      array->set_element_fast(*this, ele_idx, *val);
    }
  }

  sp = sp - elements_cnt;
}

Completion NjsVM::get_prop_on_primitive(JSValue& obj, JSValue key) {
  switch (obj.tag) {
    case JSValue::BOOLEAN:
      return boolean_prototype.as_object->get_property(*this, key);
    case JSValue::NUM_FLOAT:
      return number_prototype.as_object->get_property(*this, key);
    case JSValue::SYMBOL:
      return undefined;
      break;
    case JSValue::STRING: {
      PrimitiveString& str = *obj.as_prim_string;
      JSValue prop_key = TRYCC(js_to_property_key(*this, key));
      u32 atom = prop_key.as_atom;

      if (atom_is_int(atom)) {
        u32 idx = atom_get_int(atom);
        if (idx < str.length()) {
          return new_primitive_string(str[idx]);
        } else {
          return undefined; 
        }
      }
      else if (atom == AtomPool::k_length) {
        return JSFloat(str.length());
      }
      else {
        return string_prototype.as_object->get_prop(*this, prop_key);
      }
      break;
    }
    default:
      assert(false);
  }
  __builtin_unreachable();
}

Completion NjsVM::get_prop_common(JSValue obj, JSValue key) {
  if(obj.is_object()) [[likely]] {
    return obj.as_object->get_property(*this, key);
  }
  else if (not obj.is_nil()) {
    return get_prop_on_primitive(obj, key);
  }
  else {
    return CompThrow(build_cannot_access_prop_error(key, obj, false));
  }
}

void NjsVM::exec_get_prop_atom(SPRef sp, u32 key_atom, int keep_obj) {
  auto comp = get_prop_common(sp[0], JSAtom(key_atom));
  sp[keep_obj] = comp.get_value();
  sp += keep_obj;

  if (comp.is_throw()) [[unlikely]] {
    error_handle(sp);
  }
}

void NjsVM::exec_get_prop_index(SPRef sp, int keep_obj) {
  int res_index = keep_obj - 1;
  JSValue& index = sp[0];
  JSValue& obj = sp[-1];

  Completion comp = get_prop_common(obj, index);
  sp[res_index] = comp.get_value();
  sp += res_index;

  if (comp.is_throw()) [[unlikely]] {
    error_handle(sp); 
  }
}

Completion NjsVM::set_prop_common(JSValue obj, JSValue key, JSValue value) {
  if (obj.is_object()) {
    auto res = TRY_COMP(obj.as_object->set_property(*this, key, value));
    return undefined;
  }
  else if (obj.is_nil()) {
    return CompThrow(build_cannot_access_prop_error(key, obj, true));
  }
  // else the obj is a primitive type. do nothing.
  return undefined;
}

// top of stack: value, obj
void NjsVM::exec_set_prop_atom(SPRef sp, u32 key_atom) {
  JSValue& val = sp[0];
  JSValue& obj = sp[-1];

  auto comp = set_prop_common(obj, JSAtom(key_atom), val);
  if (comp.is_throw()) {
    val = comp.get_value();
    error_handle(sp);
  } else {
    // keep the value on the stack
    obj = val;
    sp -= 1;
  }
}

// top of stack: value, index, obj
void NjsVM::exec_set_prop_index(SPRef sp) {
  JSValue& val = sp[0];
  JSValue& index = sp[-1];
  JSValue& obj = sp[-2];

  auto comp = set_prop_common(obj, index, val);
  if (comp.is_throw()) {
    val = comp.get_value();
    error_handle(sp);
  } else {
    // keep the value on the stack
    obj = val;
    sp -= 2;
  }
}

void NjsVM::exec_dynamic_get_var(SPRef sp, u32 name_atom, bool no_throw) {
  auto comp = global_object.as_object->get_property_impl(*this, JSAtom(name_atom));
  sp[1] = comp.get_value();
  sp += 1;

  if (comp.is_throw()) [[unlikely]] {
    error_handle(sp);
  } else if (!no_throw && sp[0].flag_bits == FLAG_NOT_FOUND) [[unlikely]] {
    sp -= 1;
    error_throw_handle(sp, JS_REFERENCE_ERROR,
                       u16string(atom_to_str(name_atom)) + u" is undefined");
  }
}

void NjsVM::exec_dynamic_set_var(SPRef sp, u32 name_atom) {
  auto comp = global_object.as_object->set_prop(*this, JSAtom(name_atom), sp[0]);
  assert(comp.is_value());
}

Completion NjsVM::for_of_get_iterator(JSValue obj) {
  NoGC nogc(*this);
  auto build_err = [&, this] () {
    JSValue err = build_error(
        JS_TYPE_ERROR, to_u16string(obj.to_string(*this)) + u" is not iterable");
    return CompThrow(err);
  };
  if (obj.is_nil()) [[unlikely]] {
    return build_err();
  }
  obj = js_to_object(*this, obj).get_value();
  assert(obj.is_object());
  auto iter_key = JSSymbol(AtomPool::k_sym_iterator);
  JSValue iter_ctor = TRYCC(obj.as_object->get_property(*this, iter_key));

  if (!iter_ctor.is_object() || object_class(iter_ctor) != CLS_FUNCTION) [[unlikely]] {
    return build_err();
  }
  return call_function(iter_ctor, obj, undefined, {});
}

Completion NjsVM::for_of_call_next(JSValue iter) {
  NoGC nogc(*this);
  assert(iter.is_object());
  auto atom_next = JSAtom(AtomPool::k_next);
  JSValue next_func = TRYCC(iter.as_object->get_property(*this, atom_next));

  if (!next_func.is_object() || object_class(next_func) != CLS_FUNCTION) {
    JSValue err = build_error(
        JS_TYPE_ERROR, to_u16string(next_func.to_string(*this)) + u" is not a function.");
    return CompThrow(err);
  }

  return call_function(next_func, iter, undefined, {});
}

void NjsVM::exec_abstract_equality(SPRef sp, bool flip) {
  sp -= 1;
  JSValue& lhs = sp[0];
  JSValue& rhs = sp[1];

  ErrorOr<bool> res = lhs.tag == rhs.tag ? strict_equals(*this, lhs, rhs)
                                         : abstract_equals(*this, lhs, rhs);

  if (res.is_value()) {
    sp[0].set_bool(flip ^ res.get_value());
  } else {
    sp[0] = res.get_error();
    error_handle(sp);
  }
}

void NjsVM::exec_comparison(SPRef sp, OpType type) {
  sp -= 1;
  JSValue& lhs = sp[0];
  JSValue& rhs = sp[1];

  auto double_compare = [] (OpType type, double lhs, double rhs) {
    switch (type) {
      case OpType::lt: return lhs < rhs;
      case OpType::gt: return lhs > rhs;
      case OpType::le: return lhs <= rhs;
      case OpType::ge: return lhs >= rhs;
      default: assert(false);
    }
    __builtin_unreachable();
  };

  auto string_compare = [] (OpType type, PrimitiveString *lhs, PrimitiveString *rhs) {
    switch (type) {
      case OpType::lt: return *lhs < *rhs;
      case OpType::gt: return *lhs > *rhs;
      case OpType::le: return *lhs <= *rhs;
      case OpType::ge: return *lhs >= *rhs;
      default: assert(false);
    }
    __builtin_unreachable();
  };

  bool res;
  if (lhs.is_float64() && rhs.is_float64()) {
    res = double_compare(type, lhs.as_f64, rhs.as_f64);
  }
  else if (lhs.is(JSValue::STRING) && rhs.is(JSValue::STRING)) {
    res = string_compare(type, lhs.as_prim_string, rhs.as_prim_string);
  }
  else {
    JSValue prim_lhs = VM_TRY_COMP(js_to_primitive(*this, lhs));
    JSValue prim_rhs = VM_TRY_COMP(js_to_primitive(*this, rhs));
    if (prim_lhs.is(JSValue::STRING) && prim_rhs.is(JSValue::STRING)) {
      res = string_compare(type, prim_lhs.as_prim_string, prim_rhs.as_prim_string);
    } else {
      double f_lhs = VM_TRY_ERR(js_to_number(*this, prim_lhs));
      double f_rhs = VM_TRY_ERR(js_to_number(*this, prim_rhs));
      res = double_compare(type, f_lhs, f_rhs);
    }
  }

  lhs.set_bool(res);
}

void NjsVM::error_throw(SPRef sp, const u16string& msg) {
  error_throw(sp, JS_ERROR, msg);
}

void NjsVM::error_throw_handle(SPRef sp, JSErrorType type, u16string_view msg) {
  JSValue err_obj = build_error(type, msg);
  *++sp = err_obj;
  error_handle(sp);
}

void NjsVM::error_throw(SPRef sp, JSErrorType type, const u16string& msg) {
  JSValue err_obj = build_error(type, msg);
  *++sp = err_obj;
}

void NjsVM::error_handle(SPRef sp) {
  JSStackFrame *frame = curr_frame;
  u32& pc = *frame->pc_ref;

  u32 err_throw_pc = pc - 1;
  auto& catch_table = curr_frame->function.as_func->meta->catch_table;
  assert(catch_table.size() >= 1);

  if (catch_table.size() == 1) {
    pc = catch_table.back().goto_pos;
    return;
  }

  CatchEntry *catch_entry = nullptr;
  for (auto& entry : catch_table) {
    // 1.1 an error happens, and is caught by a `catch` statement
    if (entry.range_include(err_throw_pc)) {
      pc = entry.goto_pos;
      catch_entry = &entry;
      // restore the sp
      JSValue *sp_restore = frame->stack;
      *sp_restore = sp[0];
      sp = sp_restore;

      // dispose local variables
      JSValue *local_start = frame->local_vars + entry.local_var_begin;
      JSValue *local_end = frame->local_vars + entry.local_var_end;
      for (JSValue *val = local_start; val < local_end; val++) {
        val->set_undefined();
      }
      break;
    }
  }
  // 1.2 an error happens but there is no `catch`
  if (!catch_entry) {
    assert(catch_table.back().start_pos == catch_table.back().end_pos);
    pc = catch_table.back().goto_pos;
  }
}

void NjsVM::print_unhandled_error(JSValue err) {
  if (err.is_object() && object_class(err) == CLS_ERROR) {
    auto err_obj = err.as_object;
    // TODO: should not use `get_prop_trivial` here
    std::string err_msg = err_obj->get_prop_trivial(str_to_atom(u"message")).to_string(*this);
    std::string stack = err_obj->get_prop_trivial(str_to_atom(u"stack")).to_string(*this);
    printf("\033[31mUnhandled error: %s, at\n", err_msg.c_str());
    printf("%s\033[0m\n", stack.c_str());
  }
  else {
    printf("\033[31mUnhandled throw: %s\033[0m\n", err.to_string(*this).c_str());
  }
}

void NjsVM::exec_halt_err(SPRef sp, Instruction &inst) {
  if (!global_end) global_end = true;
  assert(sp > curr_frame->stack - 1);

  JSValue err_val = sp[0];
  print_unhandled_error(err_val);

  sp = curr_frame->stack - 1;

  curr_frame = curr_frame->move_to_heap();
  global_frame = curr_frame;
  if (Global::show_vm_exec_steps) {
    printf("\033[33m%-50s sp: %-3ld\033[0m\n\n", inst.description().c_str(), 0l);
  }
}

void NjsVM::show_stats() {
  if (Global::show_gc_statistics) {
    heap.stats.print();

    std::cout << "String alloc count: " << PrimitiveString::alloc_count << '\n';
    std::cout << "String concat count: " << PrimitiveString::concat_count << '\n';
    std::cout << "String fast concat count: " << PrimitiveString::fast_concat_count << '\n';
    std::cout << "After concat length total: " << PrimitiveString::concat_length << '\n';
    std::cout << "String append count: " << PrimitiveString::append_count << '\n';
    std::cout << "String fast append count: " << PrimitiveString::fast_append_count << '\n';

    std::cout << "string atomize count: " << atom_pool.stats.atomize_str_count << '\n';
    std::cout << "string static atomize count: " << atom_pool.stats.static_atomize_str_count << '\n';
  }

  if (Global::show_vm_stats) {
    printf("\nInstruction counter\n");
    for (int i = 0; i < static_cast<int>(OpType::opcode_count); i++) {
      printf("%20s : %lu\n", opcode_names[i], inst_counter[i]);
    }

    printf("\nmake function counter\n");
    vector<pair<int, int>> ordered;
    ordered.resize(make_function_counter.size());

    for (int i = 0; i < make_function_counter.size(); i++) {
      ordered[i].first = i;
      ordered[i].second = make_function_counter[i];
    }
    std::sort(ordered.begin(), ordered.end(), [] (auto& a, auto& b) {
      return a.second > b.second;
    });

    for (auto& [meta_idx, count] : ordered) {
      JSFunctionMeta *meta = func_meta[meta_idx].get();
      std::cout << "count: " << count;
      std::cout << "  meta_idx: " << meta_idx;
      if (meta->is_anonymous) {
        std::cout << "  name: (anonymous)";
      } else {
        std::cout << "  name: " << to_u8string(atom_to_str(meta->name_index));
      }
      std::cout << "  line: " << meta->source_line << '\n';
    }
  }
}
}