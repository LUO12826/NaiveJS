#include "NjsVM.h"

#include <iostream>
#include "njs/basic_types/JSHeapValue.h"
#include "njs/basic_types/PrimitiveString.h"
#include "njs/basic_types/conversion.h"
#include "njs/basic_types/testing_and_comparison.h"
#include "njs/basic_types/JSArray.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/global_var.h"
#include "njs/basic_types/JSNumberPrototype.h"
#include "njs/basic_types/JSBooleanPrototype.h"
#include "njs/basic_types/JSStringPrototype.h"
#include "njs/basic_types/JSObjectPrototype.h"
#include "njs/basic_types/JSArrayPrototype.h"
#include "njs/basic_types/JSFunctionPrototype.h"
#include "njs/basic_types/GlobalObject.h"
#include "Completion.h"

namespace njs {

NjsVM::NjsVM(CodegenVisitor& visitor)
  : heap(600, *this)
  , rt_stack(max_stack_size)
  , bytecode(std::move(visitor.bytecode))
  , runloop(*this)
  , str_pool(std::move(visitor.str_pool))
  , num_list(std::move(visitor.num_list))
  , func_meta(std::move(visitor.func_meta))
  , global_catch_table(std::move(visitor.scope_chain[0]->catch_table))
{
  init_prototypes();

  auto global_obj = heap.new_object<GlobalObject>();
  global_obj->set_prototype(object_prototype);
  global_object.set_val(global_obj);


  auto& global_sym_table = visitor.scope_chain[0]->get_symbol_table();

  for (auto& [sym_name, sym_rec] : global_sym_table) {
    if (sym_rec.var_kind == VarKind::DECL_VAR || sym_rec.var_kind == VarKind::DECL_FUNCTION) {
      global_obj->props_index_map.emplace(sv_to_atom(sym_name), sym_rec.index + frame_meta_size);
    }
  }

  str_pool.record_static_atom_count();

  stack_begin = rt_stack.data();
  stack_begin[0].val.as_function = nullptr;
  bp = stack_begin;
  global_sp = bp + visitor.scope_chain[0]->get_var_count() + frame_meta_size - 1;
  sp = global_sp;
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

  auto start_sp = sp - rt_stack.data();
  if (Global::show_vm_state) {
    std::cout << "### VM starts execution, state:\n";
    std::cout << "Stack top value: " << sp[0].description() << '\n';
    std::cout << "sp: " << start_sp << '\n';
    std::cout << "---------------------------------------------------\n";
  }

  execute();
  runloop.loop();

  auto end_sp = sp - rt_stack.data();
  if (Global::show_vm_state) {
    std::cout << "### end of execution VM state:\n";
    std::cout << "Stack top value: " << sp[0].description() << '\n';
    std::cout << "sp: " << end_sp << '\n';
    std::cout << "---------------------------------------------------\n";
  }

  if (start_sp != end_sp) {
    print_red_line("err: start sp != end_sp");
  }

//  if (!log_buffer.empty()) {
//    std::cout << "------------------------------" << std::endl << "log:" << std::endl;
//    for (auto& str: log_buffer) {
//      std::cout << str;
//    }
//  }

}

CallResult NjsVM::execute(bool stop_at_return) {
  auto saved_bp = bp;

  while (true) {
    Instruction& inst = bytecode[pc++];

    switch (inst.op_type) {
      case InstType::add:
        exec_add();
        break;
      case InstType::sub:
      case InstType::mul:
      case InstType::div:
        exec_binary(inst.op_type);
        break;
      case InstType::neg:
        assert(sp[0].is_float64());
        sp[0].val.as_f64 = -sp[0].val.as_f64;
        break;
      case InstType::add_assign:
        exec_add_assign(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::sub_assign:
      case InstType::mul_assign:
      case InstType::div_assign:
        exec_compound_assign(inst.op_type, inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::inc:
        exec_inc_or_dec(inst.operand.two.opr1, inst.operand.two.opr2, 1);
        break;
      case InstType::dec:
        exec_inc_or_dec(inst.operand.two.opr1, inst.operand.two.opr2, -1);
        break;
      case InstType::logi_and:
      case InstType::logi_or:
        exec_logi(inst.op_type);
        break;
      case InstType::logi_not: {
        bool bool_val = sp[0].bool_value();
        sp[0].set_undefined();
        sp[0].set_val(!bool_val);
        break;
      }
      case InstType::bits_and:
      case InstType::bits_or:
      case InstType::bits_xor:
        exec_bits(inst.op_type);
        break;
      case InstType::bits_not: {
        auto res = to_int32(*this, sp[0]);
        sp[0].set_undefined();
        if (res.is_value()) {
          int v = ~res.get_value();
          sp[0].set_val(double(v));
        } else {
          sp[0] = res.get_error();
          error_handle();
        }
        break;
      }
      case InstType::lsh:
      case InstType::rsh:
      case InstType::ursh:
        exec_shift(inst.op_type);
        break;
      case InstType::push:
        exec_push(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::push_check:
        exec_push_check(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::pushi:
        sp += 1;
        sp[0].set_val(inst.operand.num_float);
        break;
      case InstType::push_str:
        exec_push_str(inst.operand.two.opr1, false);
        break;
      case InstType::push_atom:
        exec_push_str(inst.operand.two.opr1, true);
        break;
      case InstType::push_bool:
        sp += 1;
        sp[0].set_val(bool(inst.operand.two.opr1));
        break;
      case InstType::push_this:
        exec_push_this(bool(inst.operand.two.opr1));
        break;
      case InstType::push_null:
        sp += 1;
        sp[0].tag = JSValue::JS_NULL;
        break;
      case InstType::push_undef:
        sp += 1;
        sp[0].tag = JSValue::UNDEFINED;
        break;
      case InstType::push_uninit:
        sp += 1;
        sp[0].tag = JSValue::UNINIT;
        break;
      case InstType::pop:
        exec_pop(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::pop_drop:
        sp[0].set_undefined();
        sp -= 1;
        break;
      case InstType::store:
        exec_store(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::prop_assign:
        exec_prop_assign();
        break;
      case InstType::var_deinit_range:
        exec_var_deinit_range(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::var_undef:
        assert(bp[inst.operand.two.opr1].tag == JSValue::UNINIT);
        bp[inst.operand.two.opr1].tag = JSValue::UNDEFINED;
        break;
      case InstType::var_dispose:
        exec_var_dispose(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::var_dispose_range:
        exec_var_dispose_range(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::dup_stack_top:
        if (sp[0].is_RCObject() && sp[0].val.as_RCObject->get_ref_count() != 0) {
          sp[0].val.as_RCObject = sp[0].val.as_RCObject->copy();
        }
        break;
      case InstType::jmp:
        pc = inst.operand.two.opr1;
        break;
      case InstType::jmp_true:
        if (sp[0].bool_value()) {
          pc = inst.operand.two.opr1;
        }
        break;
      case InstType::jmp_false:
        if (sp[0].is_falsy()) {
          pc = inst.operand.two.opr1;
        }
        break;
      case InstType::jmp_cond:
        if (sp[0].bool_value()) {
          pc = inst.operand.two.opr1;
        } else {
          pc = inst.operand.two.opr2;
        }
        break;
      case InstType::gt:
      case InstType::lt:
      case InstType::ge:
      case InstType::le:
        exec_comparison(inst.op_type);
        break;
      case InstType::ne:
        exec_abstract_equality(true);
        break;
      case InstType::eq:
        exec_abstract_equality(false);
        break;
      case InstType::ne3:
        exec_strict_equality(true);
        break;
      case InstType::eq3:
        exec_strict_equality(false);
        break;
      case InstType::call: {
        CallResult res = exec_call(inst.operand.two.opr1, bool(inst.operand.two.opr2));
        if (res == CallResult::DONE_ERROR) {
          error_handle();
        }
        break;
      }
      case InstType::js_new:
        exec_js_new(inst.operand.two.opr1);
        break;
      case InstType::fast_add:
        exec_fast_add(inst);
        break;
      case InstType::fast_assign:
        exec_fast_assign(inst);
        break;
      case InstType::make_func:
        exec_make_func(inst.operand.two.opr1);
        break;
      case InstType::capture:
        exec_capture(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::make_obj:
        exec_make_object();
        break;
      case InstType::make_array:
        exec_make_array(inst.operand.two.opr1);
        break;
      case InstType::add_props:
        exec_add_props(inst.operand.two.opr1);
        break;
      case InstType::add_elements:
        exec_add_elements(inst.operand.two.opr1);
        break;
      case InstType::ret:
        exec_return();
        if (stop_at_return && bp < saved_bp) return CallResult::DONE_NORMAL;
        break;
      case InstType::ret_err: {
        exec_return_error();
        if (stop_at_return && bp < saved_bp) return CallResult::DONE_ERROR;
        error_handle();
        break;
      }
      case InstType::halt:
        if (!global_end) {
          global_end = true;
          assert(global_sp == sp);
        }
        // return from a task
        else {
          assert(global_sp + 1 == sp);
          sp[0].set_undefined();
          sp -= 1;
        }
        if (Global::show_vm_exec_steps) {
          printf("\033[33m%-50s sp: %-3ld   pc: %-3u\033[0m\n\n", inst.description().c_str(), sp - rt_stack.data(), pc);
        }
        return CallResult::DONE_NORMAL;
      case InstType::halt_err:
        exec_halt_err(inst);
        return CallResult::DONE_NORMAL;
      case InstType::nop:
        break;
      case InstType::key_access:
        exec_key_access(inst.operand.two.opr1, (bool)inst.operand.two.opr2);
        break;
      case InstType::index_access:
        exec_index_access((bool)inst.operand.two.opr1);
        break;
      case InstType::dyn_get_var:
        exec_dynamic_get_var(inst.operand.two.opr1, (bool)inst.operand.two.opr2);
        break;
      default:
        assert(false);
    }
    if (Global::show_vm_exec_steps) {
      printf("%-50s sp: %-3ld   pc: %-3u\n", inst.description().c_str(), sp - rt_stack.data(), pc);
    }
  }
}

void NjsVM::execute_task(JSTask& task) {
  // let the pc point to `halt`
  pc = bytecode.size() - 2;

  prepare_for_call(task.task_func.val.as_function, task.args, nullptr);
  CallResult res = exec_call((int)task.args.size(), false);
  if (res == CallResult::UNFINISHED) {
    execute(false);
  } else if (res == CallResult::DONE_ERROR) {
    error_handle();
  }
}

Completion NjsVM::call_function(JSFunction *func, const std::vector<JSValue>& args, JSObject *this_obj) {
  prepare_for_call(func, args, this_obj);
  CallResult res = exec_call((int)args.size(), this_obj != nullptr);
  if (res == CallResult::UNFINISHED) {
    res = execute(true);
  }
  assert(res != CallResult::UNFINISHED);

  if (res == CallResult::DONE_NORMAL) {
    return pop_stack();
  } else {
    return Completion::with_throw(pop_stack());
  }
}

void NjsVM::prepare_for_call(JSFunction *func, const std::vector<JSValue>& args, JSObject *this_obj) {
  sp += 1;
  sp[0].set_val(func);
  for (auto& arg : args) {
    sp += 1;
    sp[0] = arg;
  }

  if (this_obj) invoker_this.set_val(this_obj);
}

void NjsVM::pop_drop() {
  sp[0].set_undefined();
  sp -= 1;
}

JSValue NjsVM::peek_stack_top() {
  return sp[0];
}

void NjsVM::push_stack(JSValue val) {
  sp += 1;
  sp[0] = val;
}

JSValue NjsVM::pop_stack() {
  sp -= 1;
  // want to make sure that move constructor is called.
  return std::move(sp[1]);
}

JSValue& NjsVM::stack_get_at_index(size_t index) {
  return rt_stack[index];
}

using StackTraceItem = NjsVM::StackTraceItem;

std::vector<StackTraceItem> NjsVM::capture_stack_trace() {
  std::vector<StackTraceItem> trace;

  JSValue *frame_ptr = bp;

  while (frame_ptr != stack_begin) {
    assert(frame_ptr->is(JSValue::STACK_FRAME_META1));
    auto *func = frame_ptr->val.as_function;

    trace.emplace_back(StackTraceItem {
        .func_name = func->meta.is_anonymous ? u"(anonymous)" : func->name,
        .source_line = func->meta.source_line,
        .is_native = func->meta.is_native,
    });

    // visit the deeper stack frame
    frame_ptr = frame_ptr[1].val.as_JSValue;
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

JSValue& NjsVM::get_value(ScopeType scope, int index) {
  if (scope == ScopeType::FUNC) {
    JSValue &val = bp[index];
    return val.tag != JSValue::HEAP_VAL ? val : val.deref_heap();
  }
  if (scope == ScopeType::FUNC_PARAM) {
    assert(bp[1].tag == JSValue::STACK_FRAME_META2);
    JSValue &val = bp[index - (int)func_arg_count];
    return val.tag != JSValue::HEAP_VAL ? val : val.deref_heap();
  }
  if (scope == ScopeType::GLOBAL) {
    JSValue &val = stack_begin[index];
    return val.tag != JSValue::HEAP_VAL ? val : val.deref_heap();
  }
  if (scope == ScopeType::CLOSURE) {
    return function_env()->captured_var[index].deref_heap();
  }
  __builtin_unreachable();
}

JSFunction *NjsVM::function_env() {
  assert(bp[0].tag == JSValue::STACK_FRAME_META1);
  return bp[0].val.as_function;
}

void NjsVM::exec_add() {
  if (sp[-1].is_float64() && sp[0].is_float64()) {
    sp[-1].val.as_f64 += sp[0].val.as_f64;
    sp[0].set_undefined();
    sp -= 1;
    return;
  }
  else if (sp[-1].is(JSValue::STRING) && sp[0].is(JSValue::STRING)) {
    auto *new_str = new PrimitiveString(sp[-1].val.as_primitive_string->str
                                        + sp[0].val.as_primitive_string->str);
    sp[-1].set_undefined();
    sp[-1].set_val(new_str);
  }
  sp[0].set_undefined();
  sp -= 1;
}

void NjsVM::exec_add_assign(int scope, int index) {
  JSValue& value = get_value(scope_type_from_int(scope), index);

  if (value.is_float64() && sp[0].is_float64()) {
    value.val.as_f64 += sp[0].val.as_f64;
  }
  else if (value.is(JSValue::STRING) && sp[0].is(JSValue::STRING)) {
    value.val.as_primitive_string->str += sp[0].val.as_primitive_string->str;
  }
  sp[0].set_undefined();
  sp -= 1;
}

void NjsVM::exec_compound_assign(InstType type, int scope, int index) {
  JSValue& value = get_value(scope_type_from_int(scope), index);
  // fast path
  if (value.is_float64() && sp[0].is_float64()) {
    switch (type) {
      case InstType::sub_assign: value.val.as_f64 -= sp[0].val.as_f64; break;
      case InstType::mul_assign: value.val.as_f64 *= sp[0].val.as_f64; break;
      case InstType::div_assign: value.val.as_f64 /= sp[0].val.as_f64; break;
      default: assert(false);
    }
  }
  else {
    assert(false);
  }
  sp[0].set_undefined();
  sp -= 1;
}

void NjsVM::exec_inc_or_dec(int scope, int index, int inc) {
  JSValue& value = get_value(scope_type_from_int(scope), index);
  assert(value.is_float64());
  value.val.as_f64 += inc;
}

void NjsVM::exec_binary(InstType op_type) {
  assert(sp[-1].is_float64() && sp[0].is_float64());
  switch (op_type) {
    case InstType::sub:
      sp[-1].val.as_f64 -= sp[0].val.as_f64;
      break;
    case InstType::mul:
      sp[-1].val.as_f64 *= sp[0].val.as_f64;
      break;
    case InstType::div:
      sp[-1].val.as_f64 /= sp[0].val.as_f64;
      break;
    default:
      assert(false);
  }
  sp[0].set_undefined();
  sp -= 1;
}

void NjsVM::exec_logi(InstType op_type) {
  switch (op_type) {
    case InstType::logi_and:
      if (!sp[-1].is_falsy()) {
        sp[-1].set_undefined();
        sp[-1] = std::move(sp[0]);
      }
      break;
    case InstType::logi_or:
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

void NjsVM::exec_bits(njs::InstType op_type) {

  ErrorOr<uint32_t> lhs, rhs;

  lhs = to_uint32(*this, sp[-1]);
  if (lhs.is_error()) goto error;

  rhs = to_uint32(*this, sp[0]);
  if (rhs.is_error()) goto error;

  sp[-1].set_undefined();
  sp[0].set_undefined();

  switch (op_type) {
    case InstType::bits_and:
      sp[-1].set_val(double(lhs.get_value() & rhs.get_value()));
      break;
    case InstType::bits_or:
      sp[-1].set_val(double(lhs.get_value() | rhs.get_value()));
      break;
    case InstType::bits_xor:
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
  error_handle();
}

void NjsVM::exec_shift(InstType op_type) {
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
    case InstType::lsh:
      sp[-1].set_val(double(lhs.get_value() << shift_len));
      break;
    case InstType::rsh:
      sp[-1].set_val(double(lhs.get_value() >> shift_len));
      break;
    case InstType::ursh:
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
  error_handle();
}

void NjsVM::exec_fast_add(Instruction& inst) {

  auto lhs_scope = scope_type_from_int(inst.operand.four.opr1);
  auto rhs_scope = scope_type_from_int(inst.operand.four.opr3);
  auto lhs_index = inst.operand.four.opr2;
  auto rhs_index = inst.operand.four.opr4;

  JSValue& lhs_val = get_value(lhs_scope, lhs_index);
  JSValue& rhs_val = get_value(rhs_scope, rhs_index);

  if (lhs_val.is_float64() && rhs_val.is_float64()) {
    sp += 1;
    sp[0].set_val(lhs_val.val.as_f64 + rhs_val.val.as_f64);
  }
  else {
    // currently not support.
    assert(false);
  }
}

void NjsVM::exec_return() {
  JSValue *old_sp = bp - func_arg_count - 1;

  // retain the return value, in case it's deallocated due to the dispose stage bellow.
  JSValue& ret_val = sp[0];
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->retain();

  // dispose function local storage
  for (JSValue *val = old_sp + 1; val < &ret_val; val++) {
    val->dispose();
  }
  // but the return value is actually a temporary value, so mark it as temp
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->mark_as_temp();

  // restore old state
  sp = old_sp;
  pc = bp[0].flag_bits;
  bp = bp[1].val.as_JSValue;
  func_arg_count = bp[1].flag_bits;

  // move the return value.
  sp[0] = std::move(ret_val);
}

void NjsVM::exec_return_error() {
  JSValue *old_sp = bp - func_arg_count - 1;
  JSValue *local_var_end = bp + function_env()->meta.local_var_count + frame_meta_size;

  // retain the return value, in case it's deallocated due to the dispose stage bellow.
  JSValue& ret_val = sp[0];
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->retain();

  // dispose function local storage
  for (JSValue *val = old_sp + 1; val < local_var_end; val++) {
    val->dispose();
  }
  // dispose the operand stack
  // TODO: check correctness
  for (JSValue *addr = local_var_end; addr < &ret_val; addr++) {
    addr->set_undefined();
  }
  // but the return value is actually a temporary value, so mark it as temp
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->mark_as_temp();

  // restore old state
  sp = old_sp;
  pc = bp[0].flag_bits;
  bp = bp[1].val.as_JSValue;
  func_arg_count = bp[1].flag_bits;

  // move the return value
  sp[0] = std::move(ret_val);
}

void NjsVM::exec_push(int scope, int index) {
  sp += 1;
  sp[0] = get_value(scope_type_from_int(scope), index);
}

void NjsVM::exec_push_check(int scope, int index) {
  JSValue& val = get_value(scope_type_from_int(scope), index);
  if (unlikely(val.is_uninited())) {
    error_throw(u"Cannot access a variable before initialization");
    error_handle();
    return;
  }
  sp += 1;
  sp[0] = val;
}

void NjsVM::exec_pop(int scope, int index) {
  get_value(scope_type_from_int(scope), index).assign(sp[0]);
  sp[0].set_undefined();
  sp -= 1;
}

void NjsVM::exec_store(int scope, int index) {
  auto var_scope = scope_type_from_int(scope);
  get_value(var_scope, index).assign(sp[0]);
}

void NjsVM::exec_prop_assign() {
  assert(sp[0].tag != JSValue::VALUE_HANDLE && sp[0].tag != JSValue::HEAP_VAL);

  JSValue& target_val = sp[-1];
  if (target_val.tag == JSValue::VALUE_HANDLE) {
    target_val.val.as_JSValue->assign(sp[0]);
  }
  else {
    printf("VM warning: invalid property assign.\n");
  }

  sp[0].set_undefined();
  sp[-1].set_undefined();
  sp -= 2;
}

void NjsVM::exec_var_dispose(int scope, int index) {
  auto var_scope = scope_type_from_int(scope);
  assert(var_scope == ScopeType::FUNC || var_scope == ScopeType::GLOBAL);
  JSValue& val = var_scope == ScopeType::FUNC ? bp[index] : stack_begin[index];
  val.dispose();
}

void NjsVM::exec_var_deinit_range(int start, int end) {
  for (int i = start; i < end; i++) {
    bp[i].tag = JSValue::UNINIT;
  }
}

void NjsVM::exec_var_dispose_range(int start, int end) {
  for (int i = start; i < end; i++) {
    bp[i].dispose();
  }
}

void NjsVM::exec_make_func(int meta_idx) {
  auto& meta = func_meta[meta_idx];
  auto *func = new_function(meta);

  // if a function is an arrow function, it captures the `this` value in the environment
  // where the function is created.
  if (meta.is_arrow_func) {
    if (bp != stack_begin) {
      func->This = function_env()->This;
    } else {
      func->This = global_object;
    }
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

void NjsVM::exec_capture(int scope, int index) {
  auto var_scope = scope_type_from_int(scope);

  assert(sp[0].is(JSValue::FUNCTION));

  JSFunction& func = *sp[0].val.as_function;

  if (var_scope == ScopeType::CLOSURE) {
    JSFunction *env_func = function_env();
    assert(env_func);
    JSValue& closure_val = env_func->captured_var[index];
    closure_val.val.as_heap_val->retain();
    func.captured_var.push_back(closure_val);
  }
  else {
    JSValue* stack_val;
    if (var_scope == ScopeType::FUNC) {
      stack_val = bp + index;
    } else if (var_scope == ScopeType::FUNC_PARAM) {
      stack_val = bp + index - (int)func_arg_count;
    } else if (var_scope == ScopeType::GLOBAL) {
      stack_val = stack_begin + index;
    } else {
      assert(false);
    }

    if (stack_val->tag != JSValue::HEAP_VAL) {
      stack_val->move_to_heap();
    }
    stack_val->val.as_heap_val->retain();
    func.captured_var.push_back(*stack_val);
  }
}

CallResult NjsVM::exec_call(int arg_count, bool has_this_object) {
  JSValue& func_val = sp[-arg_count];
  assert(func_val.tag == JSValue::FUNCTION);
  assert(func_val.val.as_object->obj_class == ObjectClass::CLS_FUNCTION);
  JSFunction *func = func_val.val.as_function;

  u32 def_param_cnt = func->meta.param_count;
  // If the actually passed arguments are fewer than the formal parameters,
  // fill the vacancy with `undefined`.
  sp += def_param_cnt > arg_count ? (def_param_cnt - arg_count) : 0;
  u32 actual_arg_cnt = std::max(def_param_cnt, u32(arg_count));

  // function parameters are considered `in memory` variables. So retain the RCObjects here.
  for (JSValue *val = sp - actual_arg_cnt + 1; val <= sp; val++) {
    if (val->is_RCObject()) {
      val->val.as_RCObject->retain();
    }
  }

  if (!(func->meta.is_arrow_func || func->has_this_binding)) {
    // set up the `this` for the function.
    func->This.assign(has_this_object ? invoker_this : global_object);
    invoker_this.set_undefined();
  }

  // note that, even if the function is a native function, we set up the frame metadata
  // because we need it when capture the call stack trace.
  sp += 1;
  // first cell of a function stack frame: return address and pointer to the function object.
  sp[0].tag = JSValue::STACK_FRAME_META1;
  sp[0].flag_bits = pc;
  sp[0].val.as_function = func;

  // second cell of a function stack frame: saved `bp` and arguments count
  sp[1].tag = JSValue::STACK_FRAME_META2;
  sp[1].flag_bits = actual_arg_cnt;
  sp[1].val.as_JSValue = bp;


  if (not func->meta.is_native) {
    func_arg_count = actual_arg_cnt;
    bp = sp;
    sp = sp + frame_meta_size + func->meta.local_var_count - 1;
    pc = func->meta.code_address;

    return CallResult::UNFINISHED;
  }
  else {
    JSValue *saved_bp = bp;
    bp = sp;
    sp += 1;
    Completion comp = func->native_func(*this, *func, ArrayRef<JSValue>(&func_val + 1, actual_arg_cnt));
    // if the native function throws an error, move this error object to the place where the return value should reside.
    func_val = comp.is_throw() ? comp.get_error() : comp.get_value();

    // clean the STACK_FRAME_META1 and STACK_FRAME_META2
    sp -= 2;
    sp[1].tag = JSValue::UNDEFINED;
    sp[2].tag = JSValue::UNDEFINED;

    // clean the arguments
    for (JSValue *arg = sp - actual_arg_cnt + 1; arg <= sp; arg++) {
      arg->dispose();
    }
    sp -= actual_arg_cnt;
    bp = saved_bp;
    return comp.is_throw() ? CallResult::DONE_ERROR : CallResult::DONE_NORMAL;
  }
}

void NjsVM::exec_js_new(int arg_count) {
  JSValue& ctor = sp[-arg_count];
  assert(ctor.is(JSValue::FUNCTION));

  // prepare `this` object
  JSValue proto = ctor.val.as_function->get_prop(StringPool::ATOM_prototype, false);
  proto = proto.is_object() ? proto : object_prototype;
  auto *this_obj = heap.new_object<JSObject>(ObjectClass::CLS_OBJECT, proto);
  invoker_this.set_val(this_obj);

  // run the constructor
  CallResult call_res = exec_call(arg_count, true);
  if (call_res == CallResult::UNFINISHED) {
    call_res = execute(true);
  }
  // error happens in the constructor
  if (call_res == CallResult::DONE_ERROR) {
    error_handle();
    return;
  }

  JSValue& ret_val = sp[0];
  // if the constructor doesn't return an Object (which should be the common case),
  // set the stack top (where the return locates) to the `invoker_this` object.
  if (!ret_val.is_object()) {
    ret_val.set_undefined();
    ret_val.set_val(this_obj);
  }
  // Now the newly constructed object is on the stack top. thus set the invoker this to `undefined`.
  invoker_this.set_undefined();
}

void NjsVM::exec_make_object() {
  sp += 1;
  sp[0].set_val(new_object());
}

void NjsVM::exec_make_array(int length) {
  auto *array = heap.new_object<JSArray>(*this, length);
  sp += 1;
  sp[0].set_val(array);
}

void NjsVM::exec_fast_assign(Instruction& inst) {

  auto lhs_scope = scope_type_from_int(inst.operand.four.opr1);
  auto rhs_scope = scope_type_from_int(inst.operand.four.opr3);

  JSValue& lhs_val = get_value(lhs_scope, inst.operand.four.opr2);
  JSValue& rhs_val = get_value(rhs_scope, inst.operand.four.opr4);

  lhs_val.assign(rhs_val);
}

void NjsVM::exec_add_props(int props_cnt) {
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

void NjsVM::exec_add_elements(int elements_cnt) {
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

void NjsVM::exec_push_str(int str_idx, bool atom) {
  sp += 1;
  sp[0].tag = atom ? JSValue::JS_ATOM : JSValue::STRING;

  if (atom) {
    sp[0].val.as_i64 = str_idx;
  } else {
    auto str = new PrimitiveString(atom_to_str(str_idx));
    sp[0].val.as_primitive_string = str;
  }
}

void NjsVM::exec_push_this(bool in_global) {
  sp += 1;

  if (!in_global) {
    assert(function_env());
    sp[0] = function_env()->This;
  } else {
    sp[0] = global_object;
  }
}

void NjsVM::exec_key_access(u32 key_atom, bool get_ref) {
  JSValue& val_obj = sp[0];
  invoker_this.assign(val_obj);

  if (Global::show_vm_exec_steps) {
    std::cout << "...visit key " << to_u8string(atom_to_str(key_atom)) << '\n';
  }
  if(val_obj.is_object()) {
    if (int64_t(key_atom) == StringPool::ATOM___proto__) {
      val_obj = val_obj.val.as_object->get_prototype();
    } else {
      val_obj = val_obj.val.as_object->get_prop(key_atom, get_ref);
    }
  }
  else if (val_obj.is_uninited() || val_obj.is_undefined() || val_obj.is_null()) {
    goto error;
  }
  else if (!key_access_on_primitive(val_obj, key_atom)) {
    goto error;
  }

  if (val_obj.is_uninited()) val_obj = JSValue::undefined;
  return;
error:
  error_throw(u"cannot read property of " + to_u16string(val_obj.to_string(*this)));
  error_handle();
}

bool NjsVM::key_access_on_primitive(JSValue& obj, int64_t atom) {

  switch (obj.tag) {
    case JSValue::BOOLEAN:
    case JSValue::NUM_INT64:
    case JSValue::NUM_FLOAT:
    case JSValue::SYMBOL:
      obj.set_undefined();
      break;
    case JSValue::STRING: {
      if (atom == StringPool::ATOM_length) {
        auto len = obj.val.as_primitive_string->length();
        obj.set_undefined();
        obj.set_val(double(len));
      }
      else if (string_prototype.as_object()->has_own_property(atom)) {
        auto func_val = string_prototype.as_object()->get_prop(atom, false);
        assert(func_val.is(JSValue::FUNCTION));
        obj.set_undefined();
        obj.set_val(func_val.val.as_function);
      }
      else {
        obj.set_undefined();
      }
      break;
    }

    default:
      assert(false);
  }

  return true;
}

void NjsVM::exec_index_access(bool get_ref) {
  JSValue& index = sp[0];
  JSValue& obj = sp[-1];

  assert(index.tag == JSValue::STRING
        || index.tag == JSValue::JS_ATOM
        || index.tag == JSValue::NUM_FLOAT);
  assert(obj.is_object());

  invoker_this = obj;
  u32 index_int = u32(index.val.as_f64);

  // Index an array
  if (obj.is(JSValue::ARRAY)) {
    // The float value can be interpreted as array index
    if (index.is_float64() && index.is_integer() && index.is_non_negative()) {
      obj = obj.val.as_array->access_element(index_int, get_ref);
    }
    // in this case, the float value is interpreted as an ordinary property key
    else if (index.is_float64()) {
      u16string num_str = to_u16string(std::to_string(index.val.as_f64));
      int64_t atom = str_to_atom(num_str);
      obj = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.is(JSValue::STRING) || index.is(JSValue::JS_ATOM)) {
      auto& index_str = index.is(JSValue::STRING)
                                    ? index.val.as_primitive_string->str
                                    : atom_to_str(index.val.as_i64);

      int64_t idx_int = scan_index_literal(index_str);
      if (idx_int != -1) {
        // string can be converted to number
        obj = obj.val.as_array->access_element(u32(idx_int), get_ref);
      } else {
        // object property
        int64_t atom = index.is(JSValue::STRING) ? str_to_atom(index_str)
                                                 : (u32)index.val.as_i64;
        obj = obj.val.as_object->get_prop(atom, get_ref);
      }
    }
  }
  // Index a string
  else if (obj.is(JSValue::STRING)) {
    JSValue res;
    // The float value can be interpreted as string index
    if (index.is_float64() && index.is_integer() && index.is_non_negative()) {
      u16string& str = obj.val.as_primitive_string->str;

      if (index_int < str.size()) {
        auto new_str = new PrimitiveString(u16string(1, str[index_int]));
        res.set_val(new_str);
      }
    }
    else if (index.is(JSValue::JS_ATOM) && index.val.as_i64 == StringPool::ATOM_length) {
      res.set_val(double(obj.val.as_primitive_string->length()));
    }

    obj.set_undefined();
    obj = res;
  }
  else if (obj.is(JSValue::OBJECT)) {
    if (index.is_float64()) {
      u16string num_str = to_u16string(std::to_string(index.val.as_f64));
      int64_t atom = str_to_atom(num_str);
      obj = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.is(JSValue::STRING)) {
      int64_t atom = str_to_atom(index.val.as_primitive_string->str);
      obj = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.is(JSValue::JS_ATOM)) {
      obj = obj.val.as_object->get_prop(index.val.as_i64, get_ref);
    }
  }

  if (obj.is_uninited()) obj = JSValue::undefined;
  index.set_undefined();
  sp -= 1;
}

void NjsVM::exec_dynamic_get_var(u32 name_atom, bool get_ref) {
  auto *global_obj = static_cast<GlobalObject*>(global_object.as_object());
  JSValue val = global_obj->get_prop(*this, name_atom, get_ref);

  if (val.is_uninited()) {
    auto& var_name = atom_to_str(name_atom);
    error_throw(var_name + u" is undefined");
    error_handle();
  } else {
    sp += 1;
    sp[0] = val;
  }
}


void NjsVM::exec_strict_equality(bool flip) {
  ErrorOr<bool> res = strict_equals(*this, sp[-1], sp[0]);
  sp[-1].set_undefined();
  sp[0].set_undefined();
  sp -= 1;

  if (res.is_value()) {
    sp[0].set_val(bool(flip ^ res.get_value()));
  } else {
    sp[0] = res.get_error();
    error_handle();
  }
}

void NjsVM::exec_abstract_equality(bool flip) {
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
    error_handle();
  }
}


void NjsVM::exec_comparison(InstType type) {
  JSValue& lhs = sp[-1];
  JSValue& rhs = sp[0];

  bool res;
  if (lhs.is_float64() && rhs.is_float64()) {
    switch (type) {
      case InstType::lt: res = lhs.val.as_f64 < rhs.val.as_f64; break;
      case InstType::gt: res = lhs.val.as_f64 > rhs.val.as_f64; break;
      case InstType::le: res = lhs.val.as_f64 <= rhs.val.as_f64; break;
      case InstType::ge: res = lhs.val.as_f64 >= rhs.val.as_f64; break;
      default: assert(false);
    }
  }
  else if (lhs.is(JSValue::STRING) && rhs.is(JSValue::STRING)) {
    switch (type) {
      case InstType::lt: res = *lhs.val.as_primitive_string < *rhs.val.as_primitive_string; break;
      case InstType::gt: res = *lhs.val.as_primitive_string > *rhs.val.as_primitive_string; break;
      case InstType::le: res = *lhs.val.as_primitive_string <= *rhs.val.as_primitive_string; break;
      case InstType::ge: res = *lhs.val.as_primitive_string >= *rhs.val.as_primitive_string; break;
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

void NjsVM::error_throw(const u16string& msg) {
  JSValue err_obj = InternalFunctions::build_error_internal(*this, msg);
  sp += 1;
  sp[0] = err_obj;
}

void NjsVM::error_handle() {
  // 1. error happens in a function (or a task)
  // if the error does not happen in a task, the error position should be (pc - 1)
  u32 err_throw_pc = pc - u32(!global_end);
  auto& catch_table = bp != stack_begin ? function_env()->meta.catch_table
                                        : this->global_catch_table;
  assert(catch_table.size() >= 1);

  CatchTableEntry *catch_entry = nullptr;
  for (auto& entry : catch_table) {
    // 1.1 an error happens, and is caught by a `catch` statement
    if (entry.range_include(err_throw_pc)) {
      pc = entry.goto_pos;
      catch_entry = &entry;
      // restore the sp
      JSValue *sp_restore;
      if (bp == stack_begin) sp_restore = global_sp;
      else sp_restore = bp + function_env()->meta.local_var_count + frame_meta_size - 1;

      // dispose the operand stack if needed
      if (sp - 1 != sp_restore) {
        for (JSValue *val = sp_restore + 1; val < sp; val++) {
          val->set_undefined();
        }
        sp_restore += 1;
        *sp_restore = std::move(sp[0]);
        sp = sp_restore;
      }

      // dispose local variables
      JSValue *local_start = bp + entry.local_var_begin;
      for (JSValue *val = local_start; val < bp + entry.local_var_end; val++) {
        val->dispose();
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

void NjsVM::exec_halt_err(Instruction &inst) {
  if (!global_end) global_end = true;
  assert(sp > global_sp);

  JSValue err_val = sp[0];

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

  // dispose the operand stack
  for (JSValue *val = global_sp + 1; val <= sp; val++) {
    val->set_undefined();
  }
  sp = global_sp;
  if (Global::show_vm_exec_steps) {
    printf("\033[33m%-50s sp: %-3ld   pc: %-3u\033[0m\n\n", inst.description().c_str(), sp - rt_stack.data(), pc);
  }
}

}