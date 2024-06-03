#include "NjsVM.h"

#include <iostream>
#include <format>
#include "Completion.h"
#include "njs/basic_types/JSValue.h"
#include "njs/utils/helper.h"
#include "njs/include/libregexp/cutils.h"
#include "njs/global_var.h"
#include "njs/common/common_def.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/basic_types/JSHeapValue.h"
#include "njs/basic_types/PrimitiveString.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/testing_and_comparison.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSRegExp.h"
#include "njs/basic_types/JSNumberPrototype.h"
#include "njs/basic_types/JSBooleanPrototype.h"
#include "njs/basic_types/JSStringPrototype.h"
#include "njs/basic_types/JSObjectPrototype.h"
#include "njs/basic_types/JSArrayPrototype.h"
#include "njs/basic_types/JSFunctionPrototype.h"
#include "njs/basic_types/JSErrorPrototype.h"
#include "njs/basic_types/JSRegExpPrototype.h"
#include "njs/basic_types/JSIteratorPrototype.h"
#include "njs/basic_types/JSDatePrototype.h"
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

// required by libregexp
BOOL lre_check_stack_overflow(void *opaque, size_t alloca_size) {
  return FALSE;
}
// required by libregexp
void *lre_realloc(void *opaque, void *ptr, size_t size) {
  return realloc(ptr, size);
}

namespace njs {

NjsVM::NjsVM(CodegenVisitor& visitor)
  : heap(600, *this)
  , bytecode(std::move(visitor.bytecode))
  , runloop(*this)
  , atom_pool(std::move(visitor.atom_pool))
  , num_list(std::move(visitor.num_list))
  , func_meta(std::move(visitor.func_meta))
{
  init_prototypes();
  JSObject *global_obj = new_object();
  global_object.set_val(global_obj);

  Scope& global_scope = *visitor.scope_chain[0];
  auto& global_sym_table = global_scope.get_symbol_table();

  for (auto& [sym_name, sym_rec] : global_sym_table) {
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
}

NjsVM::~NjsVM() {
  assert(curr_frame);
  // this should be the buffer for the global scope, and it should be on the heap.
  free(curr_frame->buffer);
  delete curr_frame;
}

void NjsVM::init_prototypes() {
  auto *func_proto = heap.new_object<JSFunctionPrototype>(*this);
  function_prototype.set_val(func_proto);
  func_proto->add_methods(*this);

  object_prototype.set_val(heap.new_object<JSObjectPrototype>(*this));
  function_prototype.as_object()->set_proto(object_prototype);

  number_prototype.set_val(heap.new_object<JSNumberPrototype>(*this));
  number_prototype.as_object()->set_proto(object_prototype);

  boolean_prototype.set_val(heap.new_object<JSBooleanPrototype>(*this));
  boolean_prototype.as_object()->set_proto(object_prototype);

  string_prototype.set_val(heap.new_object<JSStringPrototype>(*this));
  string_prototype.as_object()->set_proto(object_prototype);

  array_prototype.set_val(heap.new_object<JSArrayPrototype>(*this));
  array_prototype.as_object()->set_proto(object_prototype);

  regexp_prototype.set_val(heap.new_object<JSRegExpPrototype>(*this));
  regexp_prototype.as_object()->set_proto(object_prototype);

  date_prototype.set_val(heap.new_object<JSDatePrototype>(*this));
  date_prototype.as_object()->set_proto(object_prototype);

  native_error_protos.reserve(JSErrorType::JS_NATIVE_ERROR_COUNT);
  for (int i = 0; i < JSErrorType::JS_NATIVE_ERROR_COUNT; i++) {
    JSObject *proto = heap.new_object<JSErrorPrototype>(*this, (JSErrorType)i);
    proto->set_proto(object_prototype);
    native_error_protos.emplace_back(proto);
  }
  error_prototype = native_error_protos[0];

  iterator_prototype.set_val(heap.new_object<JSIteratorPrototype>(*this));
  iterator_prototype.as_object()->set_proto(object_prototype);
}

JSObject* NjsVM::new_object(ObjClass cls) {
  return heap.new_object<JSObject>(cls, object_prototype);
}

JSObject* NjsVM::new_object(ObjClass cls, JSValue proto) {
  return heap.new_object<JSObject>(cls, proto);
}

JSFunction* NjsVM::new_function(JSFunctionMeta *meta) {
  JSFunction *func;
  if (meta->is_anonymous) {
    func = heap.new_object<JSFunction>(*this, meta);
  } else {
    func = heap.new_object<JSFunction>(*this, atom_to_str(meta->name_index), meta);
  }
  return func;
}

JSValue NjsVM::new_primitive_string(const u16string& str) {
  return JSValue(heap.new_object<PrimitiveString>(str));
}

JSValue NjsVM::new_primitive_string(u16string&& str) {
  return JSValue(heap.new_object<PrimitiveString>(std::move(str)));
}

void NjsVM::run() {

  if (Global::show_vm_state) {
    std::cout << "### VM starts execution\n";
    std::cout << "---------------------------------------------------\n";
  }

  execute_global();
  execute_pending_task();
  runloop.loop();

  if (Global::show_vm_state) {
    std::cout << "### end of execution VM\n";
    std::cout << "---------------------------------------------------\n";
  }

  std::cout << "Heap usage: " << memory_usage_readable(heap.get_heap_usage()) << '\n';
  std::cout << "Heap object count: " << heap.get_object_count() << '\n';
  heap.stats.print();

  if (Global::show_log_buffer && !log_buffer.empty()) {
    std::cout << "------------------------------" << '\n';
    std::cout << "log:" << '\n';
    for (auto& str: log_buffer) {
      std::cout << str;
    }
  }

}

void NjsVM::execute_global() {
  global_func.set_val(heap.new_object<JSFunction>(*this, &global_meta));
  call_internal(global_func, global_object, undefined, ArgRef(nullptr, 0), CallFlags());
}

// TODO: this is a very simplified implementation.
JSValue NjsVM::prepare_arguments_array(ArgRef args) {
  auto *arr = heap.new_object<JSArray>(*this, args.size());
  for (int i = 0; i < args.size(); i++) {
    arr->dense_array[i] = args[i];
  }
  return JSValue(arr);
}

Completion NjsVM::call_internal(JSValueRef callee, JSValueRef This, JSValueRef new_target,
                                ArgRef argv, CallFlags flags) {
#define this_func (callee.u.as_func)
#define this_func_meta (*(callee.u.as_func->meta))

  assert(callee.is_function());
  if (Global::show_vm_exec_steps) {
    printf("*** call function: %s\n", to_u8string(callee.u.as_func->name).c_str());
    printf("*** heap object count: %zu\n", heap.get_object_count());
  }

  // setup call stack
  JSStackFrame frame;
  frame.prev_frame = curr_frame;
  frame.function = callee;
  curr_frame = &frame;

  bool copy_argv = (argv.size() < this_func_meta.param_count) | flags.copy_args;
  size_t actual_arg_cnt = std::max(argv.size(), (size_t)this_func_meta.param_count);
  size_t args_buf_cnt = unlikely(copy_argv) ? actual_arg_cnt : 0;
  size_t alloc_cnt = args_buf_cnt + frame_meta_size +
                     this_func_meta.local_var_count + this_func_meta.stack_size;

  JSValue *buffer = (JSValue *)alloca(sizeof(JSValue) * alloc_cnt);
  JSValue *args_buf = unlikely(copy_argv) ? buffer : argv.data();

  if (copy_argv) [[unlikely]] {
    memcpy(args_buf, argv.data(), sizeof(JSValue) * argv.size());
    for (JSValue *val = args_buf + argv.size(); val < args_buf + args_buf_cnt; val++) {
      val->set_undefined();
    }
  }

  if (this_func_meta.is_native) {
    Completion comp;
    ArgRef args_ref(args_buf, actual_arg_cnt);
    if (new_target.is_object()) [[unlikely]] {
      flags.this_is_new_target = true;
      comp = this_func->native_func(*this, callee, new_target, args_ref, flags);
    } else {
      comp = this_func->native_func(*this, callee, This, args_ref, flags);
    }
    curr_frame = curr_frame->prev_frame;
    return comp;
  }

  JSValue *local_vars = buffer + args_buf_cnt;
  JSValue *stack = local_vars + this_func_meta.local_var_count + frame_meta_size;
  // Initialize local variables and operation stack to undefined
  for (JSValue *val = local_vars; val < stack; val++) {
    val->set_undefined();
  }

  if (this_func_meta.prepare_arguments_array) [[unlikely]] {
    local_vars[0] = prepare_arguments_array(argv);
  }

  JSValue *sp = stack - 1;
  u32 pc = this_func_meta.bytecode_start;

  frame.alloc_cnt = alloc_cnt;
  frame.buffer = buffer;
  frame.args_buf = args_buf;
  frame.local_vars = local_vars;
  frame.stack = stack;
  frame.sp_ref = &sp;
  frame.pc_ref = &pc;

  auto get_value = [&, this] (ScopeType scope, int index) -> JSValue& {
    if (scope == ScopeType::FUNC) {
      JSValue &val = local_vars[index];
      return likely(val.tag != JSValue::HEAP_VAL) ? val : val.deref_heap();
    }
    if (scope == ScopeType::FUNC_PARAM) {
      JSValue &val = args_buf[index];
      return likely(val.tag != JSValue::HEAP_VAL) ? val : val.deref_heap();
    }
    if (scope == ScopeType::GLOBAL) {
      JSValue &val = global_frame->local_vars[index];
      return likely(val.tag != JSValue::HEAP_VAL) ? val : val.deref_heap();
    }
    if (scope == ScopeType::CLOSURE) {
      return this_func->captured_var[index].deref_heap();
    }
    __builtin_unreachable();
    assert(false);
  };

  auto exec_capture = [&, this] (int scope, int index) {
    auto var_scope = scope_type_from_int(scope);
    assert(sp[0].is(JSValue::FUNCTION));

    if (var_scope == ScopeType::CLOSURE) [[unlikely]] {
      JSValue& closure_val = this_func->captured_var[index];
      sp[0].u.as_func->captured_var.push_back(closure_val);
    }
    else {
      JSValue* stack_val;
      if (var_scope == ScopeType::FUNC) {
        stack_val = local_vars + index;
      } else if (var_scope == ScopeType::FUNC_PARAM) {
        stack_val = args_buf + index;
      } else if (var_scope == ScopeType::GLOBAL) {
        stack_val = &global_frame->local_vars[index];
      } else {
        assert(false);
      }

      if (stack_val->tag != JSValue::HEAP_VAL) {
        stack_val->move_to_heap(*this);
      }
      sp[0].u.as_func->captured_var.push_back(*stack_val);
    }
  };

#define GET_SCOPE (scope_type_from_int(inst.operand.two[0]))
#define OPR1 (inst.operand.two[0])
#define OPR2 (inst.operand.two[1])

  while (true) {
    Instruction& inst = bytecode[pc++];

    switch (inst.op_type) {
      case OpType::init:
        global_frame = curr_frame;
        break;
      case OpType::neg:
        assert(sp[0].is_float64());
        sp[0].u.as_f64 = -sp[0].u.as_f64;
        break;
      case OpType::add:
        exec_add(sp);
        break;
      case OpType::sub:
      case OpType::mul:
      case OpType::div:
      case OpType::mod:
        exec_binary(sp, inst.op_type);
        break;
      case OpType::inc: {
        JSValue& value = get_value(GET_SCOPE, OPR2);
        assert(value.is_float64());
        value.u.as_f64 += 1;
        break;
      }
      case OpType::dec: {
        JSValue& value = get_value(GET_SCOPE, OPR2);
        assert(value.is_float64());
        value.u.as_f64 -= 1;
        break;
      }
      case OpType::add_assign:
      case OpType::add_assign_keep:
        exec_add_assign(sp, get_value(GET_SCOPE, OPR2), inst.op_type == OpType::add_assign_keep);
        break;
      case OpType::add_to_left: {
        sp -= 1;
        JSValue& l = sp[0];
        JSValue& r = sp[1];
        if (l.is_float64() && r.is_float64()) {
          l.u.as_f64 += r.u.as_f64;
        } else if (l.is_prim_string() && r.is_prim_string()) {
          l.u.as_prim_string->str += r.u.as_prim_string->str;
        } else {
          bool succeeded;
          exec_add_common(sp, l, l, r, succeeded);
        }
        break;
      }
      case OpType::logi_and:
        sp -= 1;
        if (sp[0].bool_value()) {
          sp[0] = sp[1];
        }
        break;
      case OpType::logi_or:
        sp -= 1;
        if (sp[0].is_falsy()) {
          sp[0] = sp[1];
        }
        break;
      case OpType::logi_not: {
        bool bool_val = sp[0].bool_value();
        sp[0].set_bool(!bool_val);
        break;
      }
      case OpType::bits_and:
      case OpType::bits_or:
      case OpType::bits_xor:
        exec_bits(sp, inst.op_type);
        break;
      case OpType::bits_not: {
        auto res = js_to_int32(*this, sp[0]);
        if (res.is_value()) {
          int v = ~res.get_value();
          sp[0].set_float(v);
        } else {
          sp[0] = res.get_error();
          error_handle(sp);
        }
        break;
      }
      case OpType::lsh:
      case OpType::rsh:
      case OpType::ursh:
        exec_shift(sp, inst.op_type);
        break;

#define CHECK_UNINIT {                                                                             \
          if (sp[0].is_uninited()) [[unlikely]] {                                                  \
            sp -= 1;                                                                               \
            error_throw(sp, JS_REFERENCE_ERROR, u"Cannot access a variable before initialization");\
            error_handle(sp);                                                                      \
          }                                                                                        \
        }

#define DEREF_HEAP_IF_NEEDED \
          *++sp = (val.tag == JSValue::HEAP_VAL ? val.u.as_heap_val->wrapped_val : val);

      case OpType::push_local: {
        JSValue val = local_vars[OPR1];
        DEREF_HEAP_IF_NEEDED
        break;
      }
      case OpType::push_local_check: {
        JSValue val = local_vars[OPR1];
        DEREF_HEAP_IF_NEEDED
        CHECK_UNINIT
        break;
      }
      case OpType::push_global: {
        JSValue val = global_frame->local_vars[OPR1];
        DEREF_HEAP_IF_NEEDED
        break;
      }
      case OpType::push_global_check: {
        JSValue val = global_frame->local_vars[OPR1];
        DEREF_HEAP_IF_NEEDED
        CHECK_UNINIT
        break;
      }
      case OpType::push_arg: {
        JSValue val = args_buf[OPR1];
        DEREF_HEAP_IF_NEEDED
        break;
      }
      case OpType::push_arg_check: {
        JSValue val = args_buf[OPR1];
        DEREF_HEAP_IF_NEEDED
        CHECK_UNINIT
        break;
      }
      case OpType::push_closure:
        *++sp = this_func->captured_var[OPR1].deref_heap();
        break;
      case OpType::push_closure_check:
        *++sp = this_func->captured_var[OPR1].deref_heap();
        CHECK_UNINIT
        break;
      case OpType::push_i32:
        sp += 1;
        sp[0].tag = JSValue::NUM_INT32;
        sp[0].u.as_i32 = OPR1;
        break;
      case OpType::push_f64:
        sp += 1;
        sp[0].set_float(inst.operand.num_float);
        break;
      case OpType::push_str:
        sp += 1;
        sp[0].tag = JSValue::STRING;
        sp[0].u.as_prim_string =
            heap.new_object<PrimitiveString>(atom_to_str(OPR1));
        break;
      case OpType::push_atom:
        sp += 1;
        sp[0].tag = JSValue::JS_ATOM;
        sp[0].u.as_atom = OPR1;
        break;
      case OpType::push_bool:
        (*++sp).set_bool(OPR1);
        break;
      case OpType::push_func_this:
        *++sp = This;
        break;
      case OpType::push_global_this:
        *++sp = global_object;
        break;
      case OpType::push_null:
        (*++sp).tag = JSValue::JS_NULL;
        break;
      case OpType::push_undef:
        (*++sp).tag = JSValue::UNDEFINED;
        break;
      case OpType::push_uninit:
        (*++sp).tag = JSValue::UNINIT;
        break;
      case OpType::pop: {
        get_value(GET_SCOPE, OPR2).assign(sp[0]);
        sp -= 1;
        break;
      }
      case OpType::pop_check: {
        JSValue& val = get_value(GET_SCOPE, OPR2);
        sp -= 1;
        if (val.is_uninited()) [[unlikely]] {
          error_throw_handle(sp, JS_REFERENCE_ERROR,
                             u"Cannot access a variable before initialization");
        } else {
          val.assign(sp[1]);
        }
        break;
      }
      case OpType::pop_drop:
        sp -= 1;
        break;
      case OpType::store: {
        get_value(GET_SCOPE, OPR2).assign(sp[0]);
        break;
      }
      case OpType::store_check: {
        JSValue &val = get_value(GET_SCOPE, OPR2);
        if (val.is_uninited()) [[unlikely]] {
          error_throw_handle(sp, JS_REFERENCE_ERROR,
                             u"Cannot access a variable before initialization");
        } else {
          val.assign(sp[0]);
        }
        break;
      }
      case OpType::store_curr_func:
        local_vars[OPR1] = JSValue(callee);
        break;
      case OpType::var_deinit:
        local_vars[OPR1].tag = JSValue::UNINIT;
        break;
      case OpType::var_deinit_range:
        for (int i = OPR1; i < OPR2; i++) {
          local_vars[i].tag = JSValue::UNINIT;
        }
        break;
      case OpType::var_undef:
        assert(get_value(ScopeType::FUNC, OPR1).tag == JSValue::UNINIT);
        get_value(ScopeType::FUNC, OPR1).set_undefined();
        break;
      case OpType::loop_var_renew: {
        JSValue &val = local_vars[OPR1];
        if (val.is(JSValue::HEAP_VAL)) {
          val.move_to_stack();
        }
        break;
      }
      case OpType::var_dispose: {
        int index = OPR1;
        local_vars[index].set_undefined();
        break;
      }
      case OpType::var_dispose_range:
        for (int i = OPR1; i < OPR2; i++) {
          local_vars[i].set_undefined();
        }
        break;
      case OpType::jmp:
        pc = OPR1;
        break;
      case OpType::jmp_true:
        if (sp[0].bool_value()) {
          pc = OPR1;
        }
        break;
      case OpType::jmp_false:
        if (sp[0].is_falsy()) {
          pc = OPR1;
        }
        break;
      case OpType::jmp_cond:
        if (sp[0].bool_value()) {
          pc = OPR1;
        } else {
          pc = OPR2;
        }
        break;
      case OpType::jmp_pop:
        pc = OPR1;
        sp -= 1;
        break;
      case OpType::jmp_true_pop:
        if (sp[0].bool_value()) {
          pc = OPR1;
        }
        sp -= 1;
        break;
      case OpType::jmp_false_pop:
        if (sp[0].is_falsy()) {
          pc = OPR1;
        }
        sp -= 1;
        break;
      case OpType::jmp_cond_pop:
        if (sp[0].bool_value()) {
          pc = OPR1;
        } else {
          pc = OPR2;
        }
        sp -= 1;
        break;
      case OpType::gt:
      case OpType::lt:
      case OpType::ge:
      case OpType::le:
        exec_comparison(sp, inst.op_type);
        break;
      case OpType::ne:
        exec_abstract_equality(sp, true);
        break;
      case OpType::eq:
        exec_abstract_equality(sp, false);
        break;
      case OpType::ne3:
        exec_strict_equality(sp, true);
        break;
      case OpType::eq3:
        exec_strict_equality(sp, false);
        break;
      case OpType::call: {
        if (Global::show_vm_exec_steps) {
          printf("%-50s sp: %-3ld   pc: %-3u\n", inst.description().c_str(), (sp - (stack - 1)), pc);
        }
        exec_call(sp, OPR1, bool(OPR2), false, undefined);
        heap.gc_if_needed();
        break;
      }
      case OpType::proc_call:
        sp[1].tag = JSValue::PROC_META;
        sp[1].flag_bits = pc;
        sp += 1;
        pc = OPR1;
        break;
      case OpType::proc_ret:
        assert(sp[0].is(JSValue::PROC_META));
        pc = sp[0].flag_bits;
        sp -= 1;
        break;
      case OpType::js_new:
        exec_js_new(sp, OPR1);
        break;
      case OpType::make_func:
        exec_make_func(sp, OPR1, This);
        break;
      case OpType::capture:
        exec_capture(OPR1, OPR2);
        break;
      case OpType::make_obj:
        sp += 1;
        sp[0].set_val(new_object());
        break;
      case OpType::make_array:
        sp += 1;
        sp[0].set_val(heap.new_object<JSArray>(*this, OPR1));
        break;
      case OpType::add_props:
        exec_add_props(sp, OPR1);
        break;
      case OpType::add_elements:
        exec_add_elements(sp, OPR1);
        break;
      case OpType::ret: {
        if (Global::show_vm_exec_steps) printf("ret\n");
        curr_frame = curr_frame->prev_frame;
        return sp[0];
      }
      case OpType::ret_undef: {
        if (Global::show_vm_exec_steps) printf("ret_undef\n");
        curr_frame = curr_frame->prev_frame;
        return undefined;
      }
      case OpType::ret_err: {
        if (Global::show_vm_exec_steps) printf("ret_err\n");
        curr_frame = curr_frame->prev_frame;
        return CompThrow(sp[0]);
      }
      case OpType::halt:
        if (!global_end) {
          global_end = true;
          curr_frame = curr_frame->move_to_heap();
          global_frame = curr_frame;
          if (Global::show_vm_exec_steps) {
            size_t sp_offset = sp - (curr_frame->stack - 1);
            printf("\033[33m%-50s sp: %-3ld   pc: %-3u\033[0m\n\n", inst.description().c_str(), sp_offset, pc);
          }
          return undefined;
        }
        else {
          assert(false);
        }
      case OpType::halt_err:
        exec_halt_err(sp, inst);
        return undefined;
      case OpType::nop:
        break;
      case OpType::get_prop_atom:
      case OpType::get_prop_atom2:
        exec_get_prop_atom(sp, OPR1, inst.op_type == OpType::get_prop_atom2);
        break;
      case OpType::get_prop_index:
      case OpType::get_prop_index2:
        exec_get_prop_index(sp, inst.op_type == OpType::get_prop_index2);
        break;
      case OpType::set_prop_atom:
        exec_set_prop_atom(sp, OPR1);
        break;
      case OpType::set_prop_index:
        exec_set_prop_index(sp);
        break;
      case OpType::dyn_get_var:
      case OpType::dyn_get_var_undef: {
        int no_throw = static_cast<int>(inst.op_type) - static_cast<int>(OpType::dyn_get_var);
        exec_dynamic_get_var(sp, OPR1, no_throw);
        break;
      }
      case OpType::dyn_set_var:
        exec_dynamic_set_var(sp, OPR1);
        break;
      case OpType::dup_stack_top:
        sp[1] = sp[0];
        sp += 1;
        break;
      case OpType::move_to_top1: {
        JSValue tmp = sp[-1];
        sp[-1] = sp[0];
        sp[0] = tmp;
        break;
      }
      case OpType::move_to_top2: {
        JSValue tmp = sp[-2];
        sp[-2] = sp[-1];
        sp[-1] = sp[0];
        sp[0] = tmp;
        break;
      }
      case OpType::for_in_init: {
        if (!sp[0].is_nil()) {
          sp[0] = js_to_object(*this, sp[0]).get_value();
        }
        // `build_for_object` can handle null and undefined.
        sp[0] = JSForInIterator::build_for_object(*this, sp[0]);
        break;
      }
      case OpType::for_in_next: {
        assert(sp[0].is_object());
        assert(object_class(sp[0]) == CLS_FOR_IN_ITERATOR);
        
        auto *iter = sp[0].as_object<JSForInIterator>();
        sp[1] = iter->next(*this);
        sp[2] = JSValue(sp[1].is_uninited());
        sp += 2;
        break;
      }
      case OpType::for_of_init: {
        auto comp = for_of_get_iterator(sp[0]);
        sp[0] = comp.get_value();
        if (comp.is_throw()) [[unlikely]] {
          error_handle(sp);
        }
        break;
      }
      case OpType::for_of_next: {
        assert(sp[0].is_object());
        auto comp = for_of_call_next(sp[0]);
        JSValue res = comp.get_value();
        if (comp.is_throw()) [[unlikely]] {
          sp[0] = res;
          goto for_of_next_err;
        } else if (!res.is_object()) [[unlikely]] {
          sp[0] = build_error_internal(JS_TYPE_ERROR, u"iterator result is not an object.");
          goto for_of_next_err;
        } else {
          auto res_val = res.as_object()->get_property_impl(*this, JSAtom(AtomPool::k_value));
          if (res_val.is_throw()) {
            sp[0] = res_val.get_value();
            goto for_of_next_err;
          }
          auto res_done = res.as_object()->get_property_impl(*this, JSAtom(AtomPool::k_done));
          if (res_done.is_throw()) {
            sp[0] = res_done.get_value();
            goto for_of_next_err;
          }
          sp[1] = res_val.get_value();
          sp[2] = res_done.get_value();
          sp += 2;
        }
        break;
        for_of_next_err:
          error_handle(sp);
        break;
      }
      case OpType::iter_end_jmp:
        assert(sp[0].is_bool());
        if (sp[0].u.as_bool) {
          pc = OPR1;
          // this instruction drops the `done` and `value` produced by the iterator
          // when the iteration ends.
          sp -= 2;
        } else {
          sp -= 1;
        }
        break;
      case OpType::js_in:
        exec_in(sp);
        break;
      case OpType::js_instanceof:
        exec_instanceof(sp);
        break;
      case OpType::js_typeof:
        sp[0] = exec_typeof(sp[0]);
        break;
      case OpType::js_delete:
        exec_delete(sp);
        break;
      case OpType::js_to_number: {
        auto res = js_to_number(*this, sp[0]);
        if (res.is_error()) {
          sp[0] = res.get_error();
          error_handle(sp);
        } else {
          sp[0].set_float(res.get_value());
        }
        break;
      }
      case OpType::regexp_build:
        exec_regexp_build(sp, OPR1, OPR2);
        break;
      default:
        assert(false);
    }
    if (Global::show_vm_exec_steps && inst.op_type != OpType::call) {
      auto desc = inst.description();
      if (inst.op_type == OpType::get_prop_atom || inst.op_type == OpType::get_prop_atom2) {
        desc += " (" + to_u8string(atom_to_str(OPR1)) + ")";
      }
      printf("%-50s sp: %-3ld   pc: %-3u\n", desc.c_str(), (sp - (stack - 1)), pc);

    }
  }
}

void NjsVM::execute_task(JSTask& task) {
  execute_single_task(task);
  execute_pending_task();
}

void NjsVM::execute_single_task(JSTask& task) {
  CallFlags flags { .copy_args = true };
  auto comp = call_function(
      task.task_func,
      global_object,
      undefined,
      task.args,
      flags
  );
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
                                const vector<JSValue>& args, CallFlags flags) {
  JSFunction& f = *func.u.as_func;
  const JSValue *actual_this;
  if (f.has_this_binding) {
    actual_this = &f.this_binding;
  } else {
    actual_this = This.is_undefined() ? &global_object : &This;
  }

  ArgRef args_ref(const_cast<JSValue*>(args.data()), args.size());
  return call_internal(func, *actual_this, new_target, args_ref, flags);
}

using StackTraceItem = NjsVM::StackTraceItem;

vector<StackTraceItem> NjsVM::capture_stack_trace() {
  vector<StackTraceItem> trace;

  auto iter = curr_frame;
  while (iter != global_frame) {
    auto func = iter->function.u.as_func;
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
  std::vector<NjsVM::StackTraceItem> trace = capture_stack_trace();
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

JSValue NjsVM::build_error_internal(JSErrorType type, const u16string& msg) {
  auto *err_obj = new_object(CLS_ERROR, native_error_protos[type]);
  err_obj->set_prop(*this, u"message", new_primitive_string(msg));

  u16string trace_str = build_trace_str();
  err_obj->set_prop(*this, u"stack", new_primitive_string(std::move(trace_str)));

  return JSValue(err_obj);
}

JSValue NjsVM::build_error_internal(JSErrorType type, u16string&& msg) {
  auto *err_obj = new_object(CLS_ERROR, native_error_protos[type]);
  err_obj->set_prop(*this, u"message", new_primitive_string(std::move(msg)));

  u16string trace_str = build_trace_str();
  err_obj->set_prop(*this, u"stack", new_primitive_string(std::move(trace_str)));

  return JSValue(err_obj);
}

JSValue NjsVM::exec_typeof(JSValue val) {
  switch (val.tag) {
    case JSValue::UNDEFINED:
    case JSValue::UNINIT:
      return get_string_const(AtomPool::k_undefined);
    case JSValue::JS_NULL:
      return get_string_const(AtomPool::k_object);
    case JSValue::JS_ATOM:
      return get_string_const(AtomPool::k_string);
    case JSValue::SYMBOL:
      return get_string_const(AtomPool::k_symbol);
    case JSValue::BOOLEAN:
      return get_string_const(AtomPool::k_boolean);
    case JSValue::NUM_UINT32:
    case JSValue::NUM_INT32:
    case JSValue::NUM_FLOAT:
      return get_string_const(AtomPool::k_number);
    case JSValue::STRING:
      return get_string_const(AtomPool::k_string);
    case JSValue::BOOLEAN_OBJ:
    case JSValue::NUMBER_OBJ:
    case JSValue::STRING_OBJ:
    case JSValue::OBJECT:
    case JSValue::ARRAY:
      // will this happen?
      if (object_class(val) == CLS_FUNCTION) {
        return get_string_const(AtomPool::k_function);
      } else {
        return get_string_const(AtomPool::k_object);
      }
    case JSValue::FUNCTION:
      return get_string_const(AtomPool::k_function);
    default:
      assert(false);
  }
  __builtin_unreachable();
}

void NjsVM::exec_in(SPRef sp) {
  sp -= 1;
  // sp[0] : key, sp[1] : object.
  if (not sp[1].is_object()) [[unlikely]] {
    error_throw_handle(sp, JS_TYPE_ERROR, u"rhs of `in` is not an object");
    return;
  }

  sp[0] = VM_TRY_COMP(sp[1].as_object()->has_property(*this, sp[0]));
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
  JSValue proto = o.as_object()->get_proto();
  JSValue func_prototype = VM_TRY_COMP(cls.as_object()->get_prop(*this, AtomPool::k_prototype));

  if (not func_prototype.is_object()) {
    error_throw_handle(sp, JS_TYPE_ERROR, u"");
    return;
  }

  while (!is_instance) {
    if (same_value(proto, func_prototype)) {
      is_instance = true;
    } else if (proto.is_object()) {
      proto = proto.as_object()->get_proto();
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
    bool succeeded = VM_TRY_ERR(sp[0].as_object()->delete_property(key));
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
    auto *new_str = heap.new_object<PrimitiveString>(
        lhs_s.u.as_prim_string->str + rhs_s.u.as_prim_string->str
    );
    dest.set_val(new_str);
  } else {
    double lhs_n = VM_TRY_ERR(js_to_number(*this, lhs));
    double rhs_n = VM_TRY_ERR(js_to_number(*this, rhs));
    dest.set_float(lhs_n + rhs_n);
  }
  succeeded = true;
}

void NjsVM::exec_add(SPRef sp) {
  sp -= 1;
  JSValue& l = sp[0];
  JSValue& r = sp[1];

  if (l.is_float64() && r.is_float64()) {
    l.u.as_f64 += r.u.as_f64;
  }
  else if (l.is_prim_string() && r.is_prim_string()) {
    auto *new_str = heap.new_object<PrimitiveString>(
        l.u.as_prim_string->str + r.u.as_prim_string->str
    );
    l.set_val(new_str);
  }
  else {
    bool succeeded;
    exec_add_common(sp, l, l, r, succeeded);
  }
}

void NjsVM::exec_add_assign(SPRef sp, JSValue& target, bool keep_value) {
  sp -= 1;
  JSValue& r = sp[1];

  if (target.is_float64() && r.is_float64()) {
    target.u.as_f64 += r.u.as_f64;
  } else if (target.is_prim_string() && r.is_prim_string()) {
    target.u.as_prim_string->str += r.u.as_prim_string->str;
  } else {
    bool succeeded;
    exec_add_common(sp, target, target, r, succeeded);
    if (not succeeded) [[unlikely]] {
      return;
    }
  }
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
    sp[0].u.as_f64 = calc(op_type, sp[0].u.as_f64, sp[1].u.as_f64);
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

void NjsVM::exec_make_func(SPRef sp, int meta_idx, JSValue env_this) {
  auto *meta = func_meta[meta_idx].get();
  auto *func = new_function(meta);

  // if a function is an arrow function, it captures the `this` value in the environment
  // where the function is created.
  if (meta->is_arrow_func) {
    func->has_this_binding = true;
    func->this_binding = env_this;
  }

  if (not meta->is_arrow_func) {
    JSObject *new_prototype = new_object(CLS_OBJECT);
    new_prototype->add_prop_trivial(AtomPool::k_constructor, JSValue(func));
    func->add_prop_trivial(AtomPool::k_prototype, JSValue(new_prototype));
  }

  assert(!meta->is_native);

  sp += 1;
  sp[0].set_val(func);
}


CallResult NjsVM::exec_call(SPRef sp, int argc, bool has_this, bool is_new, JSValueRef new_target) {
  JSValue& func_val = sp[-argc];
  if (func_val.tag != JSValue::FUNCTION
      || func_val.u.as_object->get_class() != CLS_FUNCTION) [[unlikely]] {
    error_throw_handle(sp, JS_TYPE_ERROR, u"value is not callable");
    return CallResult::DONE_ERROR;
  }
  JSFunction& func = *func_val.u.as_func;
  JSValue *This;
  if (func.has_this_binding) [[unlikely]] {
    This = &func.this_binding;
  } else {
    This = has_this ? &sp[-argc - 1] : &global_object;
  }
  // call the function
  ArgRef argv(&func_val + 1, argc);
  auto comp = call_internal(func_val, *This, new_target, argv, CallFlags());

  // put the return value to the correct place
  if (has_this) {
    if (is_new) [[unlikely]] {
      if (comp.get_value().is_object()) [[unlikely]] {
        sp[-argc - 1] = comp.get_value();
      }
    } else {
      sp[-argc - 1] = comp.get_value();
    }
  } else {
    func_val = comp.get_value();
  }
  sp -= (argc + int(has_this));

  if (comp.is_throw()) [[unlikely]] {
    error_handle(sp);
    return CallResult::DONE_ERROR;
  } else {
    return CallResult::DONE_NORMAL;
  }
}

void NjsVM::exec_js_new(SPRef sp, int arg_count) {
  JSValue& ctor = sp[-arg_count];
  assert(ctor.is(JSValue::FUNCTION));
  // prepare `this` object
  JSValue proto = ctor.u.as_func->get_prop_trivial(AtomPool::k_prototype);
  proto = proto.is_object() ? proto : object_prototype;
  auto *this_obj = heap.new_object<JSObject>(CLS_OBJECT, proto);

  for (int i = 1; i > -arg_count; i--) {
    sp[i] = sp[i - 1];
  }
  sp[-arg_count].set_val(this_obj);
  sp += 1;

  // run the constructor
  CallResult res = exec_call(sp, arg_count, true, true, ctor);
  if (res == CallResult::DONE_ERROR) {
    return;
  }

  JSValue& ret_val = sp[0];
  // if the constructor doesn't return an Object (which should be the common case),
  // set the stack top (where the return locates) to the `this_obj`.
  if (!ret_val.is_object()) {
    ret_val.set_val(this_obj);
  }
}

void NjsVM::exec_add_props(SPRef sp, int props_cnt) {
  JSValue& val_obj = sp[-props_cnt * 2];
  assert(val_obj.is_object());
  JSObject *object = val_obj.u.as_object;

  for (JSValue *key = sp - props_cnt * 2 + 1; key <= sp; key += 2) {
    assert(key[0].is_atom());
    object->add_prop_trivial(key[0].u.as_atom, key[1], PFlag::VECW);
  }

  sp = sp - props_cnt * 2;
}

void NjsVM::exec_add_elements(SPRef sp, int elements_cnt) {
  JSValue& val_array = sp[-elements_cnt];
  assert(val_array.tag == JSValue::ARRAY);
  JSArray *array = val_array.u.as_array;

  u32 ele_idx = 0;
  for (JSValue *val = sp - elements_cnt + 1; val <= sp; val++, ele_idx++) {
    array->dense_array[ele_idx] = *val;
  }

  sp = sp - elements_cnt;
}

Completion NjsVM::get_prop_on_primitive(JSValue& obj, JSValue key) {
  switch (obj.tag) {
    case JSValue::BOOLEAN:
      return boolean_prototype.as_object()->get_property(*this, key);
    case JSValue::NUM_FLOAT:
      return number_prototype.as_object()->get_property(*this, key);
    case JSValue::SYMBOL:
      return undefined;
      break;
    case JSValue::STRING: {
      auto *str = obj.u.as_prim_string;
      Completion comp = js_to_property_key(*this, key);
      if (comp.is_throw()) return comp;

      u32 atom = comp.get_value().u.as_atom;
      if (atom_is_int(atom)) {
        u32 idx = atom_get_int(atom);
        if (idx < str->length()) {
          return JSValue(new_primitive_string(u16string(1, str->str[idx])));
        } else {
          return undefined; 
        }
      }
      else if (atom == AtomPool::k_length) {
        auto len = obj.u.as_prim_string->length();
        return JSFloat(len);
      }
      else {
        return string_prototype.as_object()->get_prop(*this, comp.get_value());
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
    return obj.as_object()->get_property(*this, key);
  }
  else if (not obj.is_nil()) {
    return get_prop_on_primitive(obj, key);
  }
  else {
    u16string prop_name;

    if (key.is_atom()) {
      prop_name = atom_to_str(key.u.as_atom);
    } else {
      Completion comp = js_to_string(*this, key);
      assert(comp.is_normal());
      prop_name = comp.get_value().u.as_prim_string->str;
    }
    u16string msg = u"cannot read property '" + prop_name + u"' of "
                    + to_u16string(obj.to_string(*this));
    return CompThrow(build_error_internal(JS_TYPE_ERROR, std::move(msg)));
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
    auto res = TRY_COMP(obj.as_object()->set_property(*this, key, value));
    return undefined;
  }
  else if (obj.is_nil()) {
    u16string prop_name;

    if (key.is_atom()) {
      prop_name = atom_to_str(key.u.as_atom);
    } else {
      Completion comp = js_to_string(*this, key);
      assert(comp.is_normal());
      prop_name = comp.get_value().u.as_prim_string->str;
    }
    u16string msg = u"cannot set property '" + prop_name + u"' of "
                    + to_u16string(obj.to_string(*this));

    return CompThrow(build_error_internal(JS_TYPE_ERROR, std::move(msg)));
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
  auto comp = global_object.as_object()->get_property_impl(*this, JSAtom(name_atom));
  sp[1] = comp.get_value();
  sp += 1;

  if (comp.is_throw()) [[unlikely]] {
    error_handle(sp);
  } else if (!no_throw && sp[0].flag_bits == FLAG_NOT_FOUND) [[unlikely]] {
    sp -= 1;
    error_throw_handle(sp, JS_REFERENCE_ERROR, atom_to_str(name_atom) + u" is undefined");
  }
}

void NjsVM::exec_dynamic_set_var(SPRef sp, u32 name_atom) {
  auto comp = global_object.as_object()->set_prop(*this, JSAtom(name_atom), sp[0]);
  assert(comp.is_value());
}

// TODO: pause GC in this function
Completion NjsVM::for_of_get_iterator(JSValue obj) {
  auto build_err = [&, this] () {
    JSValue err = build_error_internal(
        JS_TYPE_ERROR, to_u16string(obj.to_string(*this)) + u" is not iterable");
    return CompThrow(err);
  };
  if (obj.is_nil()) [[unlikely]] {
    return build_err();
  }
  obj = js_to_object(*this, obj).get_value();
  assert(obj.is_object());
  auto iter_key = JSSymbol(AtomPool::k_sym_iterator);
  JSValue iter_ctor = TRY_COMP(obj.as_object()->get_property(*this, iter_key));

  if (!iter_ctor.is_object() || object_class(iter_ctor) != CLS_FUNCTION) [[unlikely]] {
    return build_err();
  }
  return call_function(iter_ctor, obj, undefined, {});
}

// TODO: pause GC in this function
Completion NjsVM::for_of_call_next(JSValue iter) {
  assert(iter.is_object());
  auto atom_next = JSAtom(AtomPool::k_next);
  JSValue next_func = TRY_COMP(iter.as_object()->get_property(*this, atom_next));

  if (!next_func.is_object() || object_class(next_func) != CLS_FUNCTION) {
    JSValue err = build_error_internal(
        JS_TYPE_ERROR, to_u16string(next_func.to_string(*this)) + u" is not a function.");
    return CompThrow(err);
  }

  return call_function(next_func, iter, undefined, {});
}

void NjsVM::exec_strict_equality(SPRef sp, bool flip) {
  ErrorOr<bool> res = strict_equals(*this, sp[-1], sp[0]);
  sp -= 1;

  if (res.is_value()) {
    sp[0].set_bool(flip ^ res.get_value());
  } else {
    sp[0] = res.get_error();
    error_handle(sp);
  }
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
    res = double_compare(type, lhs.u.as_f64, rhs.u.as_f64);
  }
  else if (lhs.is(JSValue::STRING) && rhs.is(JSValue::STRING)) {
    res = string_compare(type, lhs.u.as_prim_string, rhs.u.as_prim_string);
  }
  else {
    JSValue prim_lhs = VM_TRY_COMP(js_to_primitive(*this, lhs));
    JSValue prim_rhs = VM_TRY_COMP(js_to_primitive(*this, rhs));
    if (prim_lhs.is(JSValue::STRING) && prim_rhs.is(JSValue::STRING)) {
      res = string_compare(type, prim_lhs.u.as_prim_string, prim_rhs.u.as_prim_string);
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

void NjsVM::error_throw_handle(SPRef sp, JSErrorType type, const u16string& msg) {
  JSValue err_obj = build_error_internal(type, msg);
  *++sp = err_obj;
  error_handle(sp);
}

void NjsVM::error_throw(SPRef sp, JSErrorType type, const u16string& msg) {
  JSValue err_obj = build_error_internal(type, msg);
  *++sp = err_obj;
}

void NjsVM::error_handle(SPRef sp) {
  JSStackFrame *frame = curr_frame;
  u32& pc = *frame->pc_ref;

  u32 err_throw_pc = pc - 1;
  auto& catch_table = curr_frame->function.u.as_func->meta->catch_table;
  assert(catch_table.size() >= 1);

  if (catch_table.size() == 1) {
    pc = catch_table.back().goto_pos;
    return;
  }

  CatchTableEntry *catch_entry = nullptr;
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
    auto err_obj = err.as_object();
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
}