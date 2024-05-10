#include "NjsVM.h"

#include <iostream>
#include "Completion.h"
#include "njs/global_var.h"
#include "njs/common/common_def.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/basic_types/JSHeapValue.h"
#include "njs/basic_types/PrimitiveString.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/testing_and_comparison.h"
#include "njs/basic_types/JSArray.h"
#include "njs/basic_types/JSNumberPrototype.h"
#include "njs/basic_types/JSBooleanPrototype.h"
#include "njs/basic_types/JSStringPrototype.h"
#include "njs/basic_types/JSObjectPrototype.h"
#include "njs/basic_types/JSArrayPrototype.h"
#include "njs/basic_types/JSFunctionPrototype.h"


namespace njs {

NjsVM::NjsVM(CodegenVisitor& visitor)
  : heap(600, *this)
  , bytecode(std::move(visitor.bytecode))
  , runloop(*this)
  , str_pool(std::move(visitor.str_pool))
  , num_list(std::move(visitor.num_list))
  , func_meta(std::move(visitor.func_meta))
  , global_catch_table(std::move(visitor.scope_chain[0]->catch_table))
{
  init_prototypes();
  JSObject *global_obj = new_object();
  global_object.set_val(global_obj);

  Scope& global_scope = *visitor.scope_chain[0];
  auto& global_sym_table = global_scope.get_symbol_table();

  for (auto& [sym_name, sym_rec] : global_sym_table) {
    if (sym_rec.var_kind == VarKind::DECL_VAR || sym_rec.var_kind == VarKind::DECL_FUNCTION) {
      global_obj->add_prop(*this, sym_name, JSValue::undefined);
    }
  }

  global_meta.name_index = str_pool.atomize(u"(global)");
  global_meta.local_var_count = global_scope.get_var_count();
  global_meta.stack_size = global_scope.get_max_stack_size();
  global_meta.param_count = 0;
  global_meta.code_address = 0;
  global_meta.source_line = 0;

  str_pool.record_static_atom_count();
}

NjsVM::~NjsVM() {
  assert(curr_frame != nullptr);
  // this should be the buffer for the global scope, and it should be on the heap.
  free(curr_frame->buffer);
  delete curr_frame;
}

void NjsVM::init_prototypes() {
  object_prototype.set_val(heap.new_object<JSObjectPrototype>(*this));

  number_prototype.set_val(heap.new_object<JSNumberPrototype>(*this));
  number_prototype.as_object()->set_prototype(object_prototype);

  boolean_prototype.set_val(heap.new_object<JSBooleanPrototype>(*this));
  boolean_prototype.as_object()->set_prototype(object_prototype);

  string_prototype.set_val(heap.new_object<JSStringPrototype>(*this));
  string_prototype.as_object()->set_prototype(object_prototype);

  array_prototype.set_val(heap.new_object<JSArrayPrototype>(*this));
  array_prototype.as_object()->set_prototype(object_prototype);

  function_prototype.set_val(heap.new_object<JSFunctionPrototype>(*this));
  function_prototype.as_object()->set_prototype(object_prototype);
}

JSObject* NjsVM::new_object(ObjectClass cls) {
  return heap.new_object<JSObject>(cls, object_prototype);
}

JSFunction* NjsVM::new_function(const JSFunctionMeta& meta) {
  JSFunction *func;
  if (meta.is_anonymous) {
    func = heap.new_object<JSFunction>(meta);
  } else {
    func = heap.new_object<JSFunction>(atom_to_str(meta.name_index), meta);
  }
  func->set_prototype(function_prototype);
  return func;
}

void NjsVM::run() {

  if (Global::show_vm_state) {
    std::cout << "### VM starts execution\n";
    std::cout << "---------------------------------------------------\n";
  }

  execute();
  execute_pending_task();
  runloop.loop();

  if (Global::show_vm_state) {
    std::cout << "### end of execution VM\n";
    std::cout << "---------------------------------------------------\n";
  }

//  if (!log_buffer.empty()) {
//    std::cout << "------------------------------" << std::endl << "log:" << std::endl;
//    for (auto& str: log_buffer) {
//      std::cout << str;
//    }
//  }

}

void NjsVM::execute() {
  auto *func = heap.new_object<JSFunction>(global_meta);
  call_internal(func, global_object, ArrayRef<JSValue>(nullptr, 0), CallFlags());
}

Completion NjsVM::call_internal(
    JSFunction *callee, JSValue this_obj, ArrayRef<JSValue> argv, CallFlags flags) {
  size_t args_buf_cnt = unlikely(flags.copy_args) ?
                        std::max(argv.size(), (size_t)callee->meta.param_count) : 0;
  size_t alloc_cnt = args_buf_cnt + frame_meta_size +
                     callee->meta.local_var_count + callee->meta.stack_size;

  JSValue *buffer = (JSValue *)alloca(sizeof(JSValue) * alloc_cnt);
  JSValue *args_buf = unlikely(flags.copy_args) ? buffer : argv.data();
  JSValue *local_vars = buffer + args_buf_cnt;
  JSValue *stack = local_vars + callee->meta.local_var_count + frame_meta_size;

  JSValue *sp = stack - 1;
  u32 pc = callee->meta.code_address;

  JSStackFrame frame {
      .prev_frame = curr_frame,
      .function = JSValue(callee),
      .alloc_cnt = alloc_cnt,
      .buffer = buffer,
      .args_buf = args_buf,
      .local_vars = local_vars,
      .stack = stack,
      .pc_ref = &pc,
  };

  stack_frames.push_back(&frame);
  curr_frame = &frame;

  if (unlikely(flags.copy_args)) {
    memcpy(args_buf, argv.data(), sizeof(JSValue) * argv.size());
    for (JSValue *val = args_buf + argv.size(); val < args_buf + args_buf_cnt; val++) {
      val->set_undefined();
    }
  }

  for (JSValue *val = local_vars; val < buffer + alloc_cnt; val++) {
    val->set_undefined();
  }

  auto get_value = [=, this] (ScopeType scope, int index) -> JSValue& {
    if (scope == ScopeType::FUNC) {
      JSValue &val = local_vars[index];
      return likely(val.tag != JSValue::HEAP_VAL) ? val : val.deref_heap();
    }
    if (scope == ScopeType::FUNC_PARAM) {
      JSValue &val = args_buf[index];
      return likely(val.tag != JSValue::HEAP_VAL) ? val : val.deref_heap();
    }
    if (scope == ScopeType::GLOBAL) {
      JSValue &val = stack_frames.front()->local_vars[index];
      return likely(val.tag != JSValue::HEAP_VAL) ? val : val.deref_heap();
    }
    if (scope == ScopeType::CLOSURE) {
      return callee->captured_var[index].deref_heap();
    }

    assert(false);
  };

  auto exec_capture = [=, &sp, this] (int scope, int index) {
    auto var_scope = scope_type_from_int(scope);
    assert(sp[0].is(JSValue::FUNCTION));
    JSFunction& func = *sp[0].val.as_function;

    if (var_scope == ScopeType::CLOSURE) {
      assert(callee);
      JSValue& closure_val = callee->captured_var[index];
      func.captured_var.push_back(closure_val);
    }
    else {
      JSValue* stack_val;
      if (var_scope == ScopeType::FUNC) {
        stack_val = local_vars + index;
      } else if (var_scope == ScopeType::FUNC_PARAM) {
        stack_val = args_buf + index;
      } else if (var_scope == ScopeType::GLOBAL) {
        stack_val = &stack_frames.front()->local_vars[index];
      } else {
        assert(false);
      }

      if (stack_val->tag != JSValue::HEAP_VAL) {
        stack_val->move_to_heap(*this);
      }
      func.captured_var.push_back(*stack_val);
    }
  };

#define GET_SCOPE (scope_type_from_int(inst.operand.two.opr1))
#define OPR1 (inst.operand.two.opr1)
#define OPR2 (inst.operand.two.opr2)

  while (true) {
    Instruction& inst = bytecode[pc++];

    switch (inst.op_type) {
      case OpType::add:
        exec_add(sp);
        break;
      case OpType::sub:
      case OpType::mul:
      case OpType::div:
        exec_binary(sp, inst.op_type);
        break;
      case OpType::neg:
        assert(sp[0].is_float64());
        sp[0].val.as_f64 = -sp[0].val.as_f64;
        break;
      case OpType::inc: {
        JSValue& value = get_value(GET_SCOPE, OPR2);
        assert(value.is_float64());
        value.val.as_f64 += 1;
        break;
      }
      case OpType::dec: {
        JSValue& value = get_value(GET_SCOPE, OPR2);
        assert(value.is_float64());
        value.val.as_f64 -= 1;
        break;
      }
      case OpType::logi_and:
      case OpType::logi_or:
        exec_logi(sp, inst.op_type);
        break;
      case OpType::logi_not: {
        bool bool_val = sp[0].bool_value();
        sp[0].set_val(!bool_val);
        break;
      }
      case OpType::bits_and:
      case OpType::bits_or:
      case OpType::bits_xor:
        exec_bits(sp, inst.op_type);
        break;
      case OpType::bits_not: {
        auto res = to_int32(*this, sp[0]);
        sp[0].set_undefined();
        if (res.is_value()) {
          int v = ~res.get_value();
          sp[0].set_val(double(v));
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
      case OpType::push:
        sp += 1;
        sp[0] = get_value(GET_SCOPE, OPR2);
        break;
      case OpType::push_check: {
        JSValue& val = get_value(GET_SCOPE, OPR2);
        if (unlikely(val.is_uninited())) {
          error_throw(sp, u"Cannot access a variable before initialization");
          error_handle(sp);
        } else {
          sp += 1;
          sp[0] = val;
        }
        break;
      }
      case OpType::pushi:
        sp += 1;
        sp[0].set_val(inst.operand.num_float);
        break;
      case OpType::push_str:
        sp += 1;
        sp[0].tag = JSValue::STRING;
        sp[0].val.as_primitive_string =
            heap.new_object<PrimitiveString>(atom_to_str(OPR1));
        break;
      case OpType::push_atom:
        sp += 1;
        sp[0].tag = JSValue::JS_ATOM;
        sp[0].val.as_i64 = OPR1;
        break;
      case OpType::push_bool:
        sp += 1;
        sp[0].set_val(bool(OPR1));
        break;
      case OpType::push_func_this:
        assert(callee);
        sp += 1;
        sp[0] = this_obj;
        break;
      case OpType::push_global_this:
        sp += 1;
        sp[0] = global_object;
        break;
      case OpType::push_null:
        sp += 1;
        sp[0].tag = JSValue::JS_NULL;
        break;
      case OpType::push_undef:
        sp += 1;
        sp[0].tag = JSValue::UNDEFINED;
        break;
      case OpType::push_uninit:
        sp += 1;
        sp[0].tag = JSValue::UNINIT;
        break;
      case OpType::pop: {
        get_value(GET_SCOPE, OPR2).assign(sp[0]);
        sp[0].set_undefined();
        sp -= 1;
        break;
      }
      case OpType::pop_check: {
        JSValue& val = get_value(GET_SCOPE, OPR2);
        if (unlikely(val.is_uninited())) {
          error_throw(sp, u"Cannot access a variable before initialization");
          error_handle(sp);
        } else {
          val.assign(sp[0]);
          sp[0].set_undefined();
          sp -= 1;
        }
        break;
      }
      case OpType::pop_drop:
        sp[0].set_undefined();
        sp -= 1;
        break;
      case OpType::store: {
        get_value(GET_SCOPE, OPR2).assign(sp[0]);
        break;
      }
      case OpType::store_check: {
        JSValue &val = get_value(GET_SCOPE, OPR2);
        if (unlikely(val.is_uninited())) {
          error_throw(sp, u"Cannot access a variable before initialization");
          error_handle(sp);
        } else {
          val.assign(sp[0]);
        }
        break;
      }
      case OpType::var_deinit_range:
        for (int i = OPR1; i < OPR2; i++) {
          local_vars[i].tag = JSValue::UNINIT;
        }
        break;
      case OpType::var_undef:
        assert(get_value(ScopeType::FUNC, OPR1).tag == JSValue::UNINIT);
        get_value(ScopeType::FUNC, OPR1).tag = JSValue::UNDEFINED;
        break;
      case OpType::var_dispose: {
        auto var_scope = GET_SCOPE;
        auto index = OPR2;
        assert(var_scope == ScopeType::FUNC || var_scope == ScopeType::GLOBAL);
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
        exec_call(sp, OPR1, bool(OPR2));
        break;
      }
      case OpType::js_new:
        exec_js_new(sp, OPR1);
        break;
      case OpType::make_func:
        exec_make_func(sp, OPR1, this_obj);
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
        stack_frames.pop_back();
        JSValue ret_val = sp[0];
        return ret_val;
      }
      case OpType::ret_err: {
        if (Global::show_vm_exec_steps) printf("ret_err\n");
        curr_frame = curr_frame->prev_frame;
        stack_frames.pop_back();
        JSValue ret_err = sp[0];
        return Completion::with_throw(ret_err);
      }
      case OpType::halt:
        if (!global_end) {
          global_end = true;
          curr_frame = curr_frame->move_to_heap();
          assert(stack_frames.size() == 1);
          stack_frames.front() = curr_frame;
          if (Global::show_vm_exec_steps) {
            size_t sp_offset = sp - (curr_frame->stack - 1);
            printf("\033[33m%-50s sp: %-3ld   pc: %-3u\033[0m\n\n", inst.description().c_str(), sp_offset, pc);
          }
          return JSValue::undefined;
        }
        else {
          assert(false);
        }
      case OpType::halt_err:
        exec_halt_err(sp, inst);
        return JSValue::undefined;
      case OpType::nop:
        break;
      case OpType::key_access:
      case OpType::key_access2: {
        int keep_obj = static_cast<int>(inst.op_type) - static_cast<int>(OpType::key_access);
        exec_key_access(sp, OPR1, (bool)OPR2, keep_obj);
        break;
      }
      case OpType::index_access:
      case OpType::index_access2: {
        int keep_obj = static_cast<int>(inst.op_type) - static_cast<int>(OpType::index_access);
        exec_index_access(sp, (bool)OPR1, keep_obj);
        break;
      }
      case OpType::set_prop_atom:
        exec_set_prop_atom(sp, OPR1);
        break;
      case OpType::set_prop_index:
        exec_set_prop_index(sp);
        break;
      case OpType::dyn_get_var:
        exec_dynamic_get_var(sp, OPR1);
        break;
      default:
        assert(false);
    }
    if (Global::show_vm_exec_steps && inst.op_type != OpType::call) {
      printf("%-50s sp: %-3ld   pc: %-3u\n", inst.description().c_str(), (sp - (stack - 1)), pc);
    }
  }
}

void NjsVM::execute_task(JSTask& task) {
  execute_single_task(task);
  execute_pending_task();
}

void NjsVM::execute_single_task(JSTask& task) {
  CallFlags flags { .copy_args = true };
  task.task_func.val.as_function->This = global_object;
  auto comp = call_function(
      task.task_func.val.as_function,
      global_object.val.as_object,
      task.args,
      flags
  );
  if (comp.is_throw()) {
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

Completion NjsVM::call_function(JSFunction *func, JSObject *this_obj,
                                const std::vector<JSValue>& args, CallFlags flags) {
  func->This = JSValue(this_obj);
  JSValue this_val = this_obj != nullptr ? JSValue(this_obj) : global_object;
  ArrayRef<JSValue> args_ref(const_cast<JSValue*>(args.data()), args.size());
  if (func->meta.is_native) {
    std::vector<JSValue> args_copy;
    if (unlikely(flags.copy_args)) {
      args_copy = args;
    }
    args_ref = ArrayRef<JSValue>(args_copy.data(), args_copy.size());
    return func->meta.native_func(*this, *func, args_ref);
  } else {
    return call_internal(func, this_val, args_ref, flags);
  }
}

using StackTraceItem = NjsVM::StackTraceItem;

std::vector<StackTraceItem> NjsVM::capture_stack_trace() {
  std::vector<StackTraceItem> trace;

  for (size_t i = stack_frames.size(); i > 0; i--) {
    auto func = stack_frames[i]->function.val.as_function;
    trace.emplace_back(StackTraceItem {
        .func_name = func->meta.is_anonymous ? u"(anonymous)" : func->name,
        .source_line = func->meta.source_line,
        .is_native = func->meta.is_native,
    });
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


void NjsVM::exec_add(SPRef sp) {
  if (sp[-1].is_float64() && sp[0].is_float64()) {
    sp[-1].val.as_f64 += sp[0].val.as_f64;
    sp[0].set_undefined();
    sp -= 1;
    return;
  }
  else if (sp[-1].is(JSValue::STRING) && sp[0].is(JSValue::STRING)) {
    auto *new_str = heap.new_object<PrimitiveString>(
        sp[-1].val.as_primitive_string->str + sp[0].val.as_primitive_string->str
    );

    sp[-1].set_undefined();
    sp[-1].set_val(new_str);
  }
  sp[0].set_undefined();
  sp -= 1;
}

void NjsVM::exec_binary(SPRef sp, OpType op_type) {
  assert(sp[-1].is_float64() && sp[0].is_float64());
  switch (op_type) {
    case OpType::sub:
      sp[-1].val.as_f64 -= sp[0].val.as_f64;
      break;
    case OpType::mul:
      sp[-1].val.as_f64 *= sp[0].val.as_f64;
      break;
    case OpType::div:
      sp[-1].val.as_f64 /= sp[0].val.as_f64;
      break;
    default:
      assert(false);
  }
  sp[0].set_undefined();
  sp -= 1;
}

void NjsVM::exec_logi(SPRef sp, OpType op_type) {
  switch (op_type) {
    case OpType::logi_and:
      if (!sp[-1].is_falsy()) {
        sp[-1].set_undefined();
        sp[-1] = std::move(sp[0]);
      }
      break;
    case OpType::logi_or:
      if (sp[-1].is_falsy()) {
        sp[-1].set_undefined();
        sp[-1] = std::move(sp[0]);
      }
      break;
    default:
      assert(false);
  }
  sp -= 1;
}

void NjsVM::exec_bits(SPRef sp, OpType op_type) {

  ErrorOr<uint32_t> lhs, rhs;

  lhs = to_uint32(*this, sp[-1]);
  if (lhs.is_error()) goto error;

  rhs = to_uint32(*this, sp[0]);
  if (rhs.is_error()) goto error;

  sp[-1].set_undefined();
  sp[0].set_undefined();

  switch (op_type) {
    case OpType::bits_and:
      sp[-1].set_val(double(lhs.get_value() & rhs.get_value()));
      break;
    case OpType::bits_or:
      sp[-1].set_val(double(lhs.get_value() | rhs.get_value()));
      break;
    case OpType::bits_xor:
      sp[-1].set_val(double(lhs.get_value() ^ rhs.get_value()));
      break;
    default:
      assert(false);
  }
  sp -= 1;
  return;

  error:
  sp[-1].set_undefined();
  sp[0].set_undefined();
  if (lhs.is_error()) sp[-1] = lhs.get_error();
  else sp[-1] = rhs.get_error();
  sp -= 1;
  error_handle(sp);
}

void NjsVM::exec_shift(SPRef sp, OpType op_type) {
  ErrorOr<int32_t> lhs;
  ErrorOr<uint32_t> rhs;
  uint32_t shift_len;

  lhs = to_int32(*this, sp[-1]);
  if (lhs.is_error()) goto error;

  rhs = to_uint32(*this, sp[0]);
  if (rhs.is_error()) goto error;

  sp[-1].set_undefined();
  sp[0].set_undefined();

  shift_len = rhs.get_value() & 0x1f;

  switch (op_type) {
    case OpType::lsh:
      sp[-1].set_val(double(lhs.get_value() << shift_len));
      break;
    case OpType::rsh:
      sp[-1].set_val(double(lhs.get_value() >> shift_len));
      break;
    case OpType::ursh:
      sp[-1].set_val(double(u32(lhs.get_value()) >> shift_len));
      break;
    default:
      assert(false);
  }
  sp -= 1;
  return;

  error:
  sp[-1].set_undefined();
  sp[0].set_undefined();
  if (lhs.is_error()) sp[-1] = lhs.get_error();
  else sp[-1] = rhs.get_error();
  sp -= 1;
  error_handle(sp);
}

void NjsVM::exec_make_func(SPRef sp, int meta_idx, JSValue env_this) {
  auto& meta = func_meta[meta_idx];
  auto *func = new_function(meta);

  // if a function is an arrow function, it captures the `this` value in the environment
  // where the function is created.
  if (meta.is_arrow_func) {
    func->has_this_binding = true;
    func->This = env_this;
  }

  if (not meta.is_arrow_func) {
    JSObject *new_prototype = new_object(ObjectClass::CLS_OBJECT);
    new_prototype->add_prop(StringPool::ATOM_constructor, JSValue(func), PropDesc::C | PropDesc::W);
    func->add_prop(StringPool::ATOM_prototype, JSValue(new_prototype), PropDesc::C | PropDesc::W);
  }

  assert(!meta.is_native);

  sp += 1;
  sp[0].set_val(func);
}


CallResult NjsVM::exec_call(SPRef sp, int arg_count, bool has_this_object) {
  JSValue& this_obj = has_this_object ? sp[-arg_count - 1] : global_object;
  JSValue& func_val = sp[-arg_count];
  JSValue *argv = &func_val + 1;
  JSValue *args_buf_start = argv;

  assert(func_val.tag == JSValue::FUNCTION);
  assert(func_val.val.as_object->obj_class == ObjectClass::CLS_FUNCTION);
  JSFunction *func = func_val.val.as_function;
  CallFlags flags;
  std::vector<JSValue> args_buf;

  u32 def_param_cnt = func->meta.param_count;
  u32 actual_arg_cnt = std::max(def_param_cnt, (u32)arg_count);
  // If the actually passed arguments are fewer than the formal parameters,
  // fill the vacancy with `undefined`.
  if (unlikely(def_param_cnt > arg_count)) {
    args_buf.resize(def_param_cnt);
    for (int i = 0; i < arg_count; i++) {
      args_buf[i] = argv[i];
    }
    args_buf_start = args_buf.data();
  }

  if (!(func->meta.is_arrow_func || func->has_this_binding)) {
    // set up the `this` for the function.
    func->This = has_this_object ? this_obj : global_object;
  }

  ArrayRef<JSValue> args_ref(args_buf_start, actual_arg_cnt);
  Completion comp;

  if (likely(not func->meta.is_native)) {
    comp = call_internal(func, this_obj, args_ref, flags);
  } else {
    comp = func->native_func(*this, *func, args_ref);
  }
  for (JSValue *val = argv; val < argv + arg_count; val++) {
    val->set_undefined();
  }
  if (has_this_object) {
    this_obj = comp.get_value();
    func_val.set_undefined();
  } else {
    func_val = comp.get_value();
  }
  sp -= (arg_count + int(has_this_object));

  if (comp.is_throw()) {
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
  JSValue proto = ctor.val.as_function->get_prop(StringPool::ATOM_prototype, false);
  proto = proto.is_object() ? proto : object_prototype;
  auto *this_obj = heap.new_object<JSObject>(ObjectClass::CLS_OBJECT, proto);

  for (int i = 1; i > -arg_count; i--) {
    sp[i] = sp[i - 1];
  }
  sp[-arg_count].set_val(this_obj);
  sp += 1;

  // run the constructor
  CallResult res = exec_call(sp, arg_count, true);
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
  JSObject *object = val_obj.val.as_object;

  for (JSValue *key = sp - props_cnt * 2 + 1; key <= sp; key += 2) {
    object->add_prop(key[0], key[1]);
    key[0].set_undefined();
    key[1].set_undefined();
  }

  sp = sp - props_cnt * 2;
}

void NjsVM::exec_add_elements(SPRef sp, int elements_cnt) {
  JSValue& val_array = sp[-elements_cnt];
  assert(val_array.tag == JSValue::ARRAY);
  JSArray *array = val_array.val.as_array;

  array->dense_array.resize(elements_cnt);

  u32 ele_idx = 0;
  for (JSValue *val = sp - elements_cnt + 1; val <= sp; val++, ele_idx++) {
    array->dense_array[ele_idx].assign(*val);
    val[0].set_undefined();
  }

  sp = sp - elements_cnt;
}


void NjsVM::exec_key_access(SPRef sp, u32 key_atom, bool get_ref, int keep_obj) {
  JSValue& val_obj = sp[0];

  if (Global::show_vm_exec_steps) {
    std::cout << "...visit key " << to_u8string(atom_to_str(key_atom)) << '\n';
  }
  if(val_obj.is_object()) {
    if (int64_t(key_atom) == StringPool::ATOM___proto__) {
      sp[keep_obj] = val_obj.val.as_object->get_prototype();
    } else {
      sp[keep_obj] = val_obj.val.as_object->get_prop(key_atom, get_ref);
    }
  }
  else if (val_obj.is_uninited() || val_obj.is_undefined() || val_obj.is_null()) {
    goto error;
  }
  else if (!key_access_on_primitive(sp, val_obj, key_atom, keep_obj)) {
    goto error;
  }

  sp += keep_obj;
  if (sp[keep_obj].is_uninited()) val_obj = JSValue::undefined;
  return;
error:
  error_throw(sp, u"cannot read property of " + to_u16string(val_obj.to_string(*this)));
  error_handle(sp);
}

bool NjsVM::key_access_on_primitive(SPRef sp, JSValue& obj, int64_t atom, int keep_obj) {

  switch (obj.tag) {
    case JSValue::BOOLEAN:
    case JSValue::NUM_INT64:
    case JSValue::NUM_FLOAT:
    case JSValue::SYMBOL:
      sp[keep_obj].set_undefined();
      break;
    case JSValue::STRING: {
      if (atom == StringPool::ATOM_length) {
        auto len = obj.val.as_primitive_string->length();
        sp[keep_obj].set_undefined();
        sp[keep_obj].set_val(double(len));
      }
      else if (string_prototype.as_object()->has_own_property(atom)) {
        auto func_val = string_prototype.as_object()->get_prop(atom, false);
        assert(func_val.is(JSValue::FUNCTION));
        sp[keep_obj].set_undefined();
        sp[keep_obj].set_val(func_val.val.as_function);
      }
      else {
        sp[keep_obj].set_undefined();
      }
      break;
    }

    default:
      assert(false);
  }

  return true;
}

void NjsVM::exec_index_access(SPRef sp, bool get_ref, int keep_obj) {
  int res_index = keep_obj - 1;
  JSValue& index = sp[0];
  JSValue& obj = sp[-1];

  JSValue prop = index_object(obj, index, get_ref);

  sp[res_index].set_undefined();
  index.set_undefined();
  sp[res_index] = prop;
  sp += res_index;
}

JSValue NjsVM::index_object(JSValue obj, JSValue index, bool get_ref) {
  assert(index.tag == JSValue::STRING
         || index.tag == JSValue::JS_ATOM
         || index.tag == JSValue::NUM_FLOAT);
  assert(obj.is_object());

  JSValue res;
  u32 index_int = u32(index.val.as_f64);

  // Index an array
  if (obj.is(JSValue::ARRAY)) {
    // The float value can be interpreted as array index
    if (index.is_float64() && index.is_integer() && index.is_non_negative()) {
      res = obj.val.as_array->access_element(index_int, get_ref);
    }
    // in this case, the float value is interpreted as an ordinary property key
    else if (index.is_float64()) {
      u16string num_str = to_u16string(std::to_string(index.val.as_f64));
      int64_t atom = str_to_atom(num_str);
      res = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.is(JSValue::STRING) || index.is(JSValue::JS_ATOM)) {
      auto& index_str = index.is(JSValue::STRING)
                        ? index.val.as_primitive_string->str
                        : atom_to_str(index.val.as_i64);

      int64_t idx_int = scan_index_literal(index_str);
      if (idx_int != -1) {
        // string can be converted to number
        res = obj.val.as_array->access_element(u32(idx_int), get_ref);
      } else {
        // object property
        int64_t atom = index.is(JSValue::STRING) ? str_to_atom(index_str)
                                                 : (u32)index.val.as_i64;
        res = obj.val.as_object->get_prop(atom, get_ref);
      }
    }
  }
  // Index a string
  else if (obj.is(JSValue::STRING)) {
    // The float value can be interpreted as string index
    if (index.is_float64() && index.is_integer() && index.is_non_negative()) {
      u16string& str = obj.val.as_primitive_string->str;

      if (index_int < str.size()) {
        auto new_str = heap.new_object<PrimitiveString>(u16string(1, str[index_int]));
        res.set_val(new_str);
      }
    }
    else if (index.is(JSValue::JS_ATOM) && index.val.as_i64 == StringPool::ATOM_length) {
      res.set_val(double(obj.val.as_primitive_string->length()));
    }
  }
  else if (obj.is(JSValue::OBJECT)) {
    if (index.is_float64()) {
      u16string num_str = to_u16string(std::to_string(index.val.as_f64));
      int64_t atom = str_to_atom(num_str);
      res = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.is(JSValue::STRING)) {
      int64_t atom = str_to_atom(index.val.as_primitive_string->str);
      res = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.is(JSValue::JS_ATOM)) {
      res = obj.val.as_object->get_prop(index.val.as_i64, get_ref);
    }
  }

  return res;
}

// top of stack: value, obj
void NjsVM::exec_set_prop_atom(SPRef sp, u32 key_atom) {
  JSValue &val = sp[0];
  JSValue &obj = sp[-1];

  if (Global::show_vm_exec_steps) {
    std::cout << "...visit key " << to_u8string(atom_to_str(key_atom)) << '\n';
  }
  if (obj.is_object()) {
    if (unlikely(int64_t(key_atom) == StringPool::ATOM___proto__)) {
      obj.val.as_object->set_prototype(val);
    } else {
      obj.val.as_object->get_prop(key_atom, true).val.as_JSValue->assign(val);
    }
  } else if (obj.is_uninited() || obj.is_undefined() || obj.is_null()) {
    error_throw(sp, u"cannot read property of " + to_u16string(obj.to_string(*this)));
    error_handle(sp);
    return;
  }
  // else the obj is a primitive type. do nothing.

  obj.set_undefined();
  obj = std::move(val);
  sp -= 1;
}

// top of stack: value, index, obj
void NjsVM::exec_set_prop_index(SPRef sp) {
  JSValue& val = sp[0];
  JSValue& index = sp[-1];
  JSValue& obj = sp[-2];

  if (obj.is_object()) {
    index_object(obj, index, true).val.as_JSValue->assign(val);
  } else if (obj.is_uninited() || obj.is_undefined() || obj.is_null()) {
    error_throw(sp, u"cannot set property of " + to_u16string(obj.to_string(*this)));
    error_handle(sp);
    return;
  }
  // else the obj is a primitive type. do nothing.

  obj.set_undefined();
  index.set_undefined();

  obj = std::move(val);
  sp -= 2;
}

void NjsVM::exec_dynamic_get_var(SPRef sp, u32 name_atom) {
  JSValue val = global_object.as_object()->get_prop(name_atom, false);

  if (val.is_uninited()) {
    auto& var_name = atom_to_str(name_atom);
    error_throw(sp, var_name + u" is undefined");
    error_handle(sp);
  } else {
    sp += 1;
    sp[0] = val;
  }
}


void NjsVM::exec_strict_equality(SPRef sp, bool flip) {
  ErrorOr<bool> res = strict_equals(*this, sp[-1], sp[0]);
  sp[-1].set_undefined();
  sp[0].set_undefined();
  sp -= 1;

  if (res.is_value()) {
    sp[0].set_val(bool(flip ^ res.get_value()));
  } else {
    sp[0] = res.get_error();
    error_handle(sp);
  }
}

void NjsVM::exec_abstract_equality(SPRef sp, bool flip) {
  JSValue& lhs = sp[-1];
  JSValue& rhs = sp[0];

  ErrorOr<bool> res = lhs.tag == rhs.tag ? strict_equals(*this, lhs, rhs)
                                         : abstract_equals(*this, lhs, rhs);
  lhs.set_undefined();
  rhs.set_undefined();
  sp -= 1;

  if (res.is_value()) {
    sp[0].set_val(bool(flip ^ res.get_value()));
  } else {
    sp[0] = res.get_error();
    error_handle(sp);
  }
}


void NjsVM::exec_comparison(SPRef sp, OpType type) {
  JSValue& lhs = sp[-1];
  JSValue& rhs = sp[0];

  bool res;
  if (lhs.is_float64() && rhs.is_float64()) {
    switch (type) {
      case OpType::lt: res = lhs.val.as_f64 < rhs.val.as_f64; break;
      case OpType::gt: res = lhs.val.as_f64 > rhs.val.as_f64; break;
      case OpType::le: res = lhs.val.as_f64 <= rhs.val.as_f64; break;
      case OpType::ge: res = lhs.val.as_f64 >= rhs.val.as_f64; break;
      default: assert(false);
    }
  }
  else if (lhs.is(JSValue::STRING) && rhs.is(JSValue::STRING)) {
    switch (type) {
      case OpType::lt: res = *lhs.val.as_primitive_string < *rhs.val.as_primitive_string; break;
      case OpType::gt: res = *lhs.val.as_primitive_string > *rhs.val.as_primitive_string; break;
      case OpType::le: res = *lhs.val.as_primitive_string <= *rhs.val.as_primitive_string; break;
      case OpType::ge: res = *lhs.val.as_primitive_string >= *rhs.val.as_primitive_string; break;
      default: assert(false);
    }
    lhs.set_undefined();
  }
  else
    assert(false);

  lhs.set_val(res);
  rhs.set_undefined();
  sp -= 1;
}

void NjsVM::error_throw(SPRef sp, const u16string& msg) {
  JSValue err_obj = InternalFunctions::build_error_internal(*this, msg);
  sp += 1;
  sp[0] = err_obj;
}

void NjsVM::error_handle(SPRef sp) {
  bool in_global = curr_frame->prev_frame == nullptr;
  auto this_func = curr_frame->function.val.as_function;
  auto frame = curr_frame;
  u32& pc = *frame->pc_ref;

  // TODO: check this
  u32 err_throw_pc = pc - 1;
  auto& catch_table = unlikely(in_global) ?
      global_catch_table : this_func->meta.catch_table;
  assert(catch_table.size() >= 1);

  CatchTableEntry *catch_entry = nullptr;
  for (auto& entry : catch_table) {
    // 1.1 an error happens, and is caught by a `catch` statement
    if (entry.range_include(err_throw_pc)) {
      pc = entry.goto_pos;
      catch_entry = &entry;
      // restore the sp
      JSValue *sp_restore = frame->stack - 1;

      // dispose the operand stack
      for (JSValue *val = sp_restore + 1; val < sp; val++) {
        val->set_undefined();
      }
      sp_restore += 1;
      *sp_restore = std::move(sp[0]);
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

void NjsVM::print_unhandled_error(JSValue err_val) {
  if (err_val.is_object() && err_val.as_object()->obj_class == ObjectClass::CLS_ERROR) {
    auto err_obj = err_val.as_object();
    std::string err_msg = err_obj->get_prop(*this, u16string_view(u"message")).to_string(*this);
    std::string stack = err_obj->get_prop(*this, u16string_view(u"stack")).to_string(*this);
    printf("\033[31mUnhandled error: %s, at\n", err_msg.c_str());
    printf("%s\033[0m\n", stack.c_str());
  }
  else {
    printf("\033[31mUnhandled throw: %s\033[0m\n", err_val.to_string(*this).c_str());
  }
}

void NjsVM::exec_halt_err(SPRef sp, Instruction &inst) {
  if (!global_end) global_end = true;
  assert(sp > curr_frame->stack - 1);

  JSValue err_val = sp[0];
  print_unhandled_error(err_val);

  // dispose the operand stack
  for (JSValue *val = curr_frame->stack; val <= sp; val++) {
    val->set_undefined();
  }
  sp = curr_frame->stack - 1;

  curr_frame = curr_frame->move_to_heap();
  assert(stack_frames.size() == 1);
  stack_frames.front() = curr_frame;
  if (Global::show_vm_exec_steps) {
    printf("\033[33m%-50s sp: %-3ld\033[0m\n\n", inst.description().c_str(), 0l);
  }
}

}