#include "NjsVM.h"

#include <iostream>
#include "njs/basic_types/JSHeapValue.h"
#include "njs/basic_types/PrimitiveString.h"
#include "njs/basic_types/JSArray.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/global_var.h"
#include "njs/basic_types/JSObjectPrototype.h"
#include "njs/basic_types/JSArrayPrototype.h"
#include "njs/basic_types/JSStringPrototype.h"
#include "njs/basic_types/JSFunctionPrototype.h"

namespace njs {

NjsVM::NjsVM(CodegenVisitor& visitor)
  : heap(600, *this)
  , rt_stack(max_stack_size)
  , bytecode(std::move(visitor.bytecode))
  , runloop(*this)
  , str_pool(std::move(visitor.str_pool))
  , num_list(std::move(visitor.num_list))
  , func_meta(std::move(visitor.func_meta))
  , global_catch_table(std::move(visitor.scope_chain[0]->get_context().catch_table))
{
  init_prototypes();
  top_level_this.set_val(new_object());

  auto global_obj = heap.new_object<GlobalObject>();
  global_obj->set_prototype(object_prototype);
  global_object.set_val(global_obj);


  auto& global_sym_table = visitor.scope_chain[0]->get_symbol_table();

  for (auto& [sym_name, sym_rec] : global_sym_table) {
    global_obj->props_index_map.emplace(u16string(sym_name), sym_rec.index + frame_meta_size);
  }

  str_pool.record_static_atom_count();

  rt_stack_begin = rt_stack.data();
  rt_stack_begin[0].val.as_function = nullptr;
  frame_base_ptr = rt_stack_begin;
  global_sp = frame_base_ptr + visitor.scope_chain[0]->get_var_count() + frame_meta_size;
  sp = global_sp;
}

void NjsVM::add_native_func_impl(u16string name, NativeFuncType func) {
  native_func_binding.emplace(std::move(name), func);
}

void NjsVM::add_builtin_object(const u16string& name,
                               const std::function<JSObject*(GCHeap&, StringPool&)>& builder) {
  GlobalObject& global_obj = *static_cast<GlobalObject *>(global_object.val.as_object);
  auto iter = global_obj.props_index_map.find(name);
  if (iter == global_obj.props_index_map.end()) return;

  u32 index = iter->second;
  rt_stack[index].set_val(builder(heap, str_pool));
}

void NjsVM::init_prototypes() {
  object_prototype.set_val(heap.new_object<JSObjectPrototype>(*this));

  array_prototype.set_val(heap.new_object<JSArrayPrototype>(*this));
  array_prototype.as_object()->set_prototype(object_prototype);

  string_prototype.set_val(heap.new_object<JSStringPrototype>(*this));
  string_prototype.as_object()->set_prototype(object_prototype);

  function_prototype.set_val(heap.new_object<JSFunctionPrototype>(*this));
  function_prototype.as_object()->set_prototype(object_prototype);
}

JSObject* NjsVM::new_object(ObjectClass cls, JSValue prototype) {
  auto *obj = heap.new_object<JSObject>(cls);
  obj->set_prototype(prototype);
  return obj;
}

JSObject* NjsVM::new_object(ObjectClass cls) {
  auto *obj = heap.new_object<JSObject>(cls);
  obj->set_prototype(object_prototype);
  return obj;
}

JSArray* NjsVM::new_array(int length) {
  auto *arr = heap.new_object<JSArray>(length);
  arr->set_prototype(array_prototype);
  return arr;
}

JSFunction* NjsVM::new_function(const JSFunctionMeta& meta) {
  JSFunction *func;
  if (meta.is_anonymous) {
    func = heap.new_object<JSFunction>(meta);
  }
  else {
    func = heap.new_object<JSFunction>(str_pool.get_string(meta.name_index), meta);
  }
  func->set_prototype(function_prototype);
  return func;
}

void NjsVM::run() {

  auto start_sp = sp - rt_stack.data();
  if (Global::show_vm_state) {
    std::cout << "### VM starts execution, state:\n";
    std::cout << "Stack top value: " << sp[-1].description() << '\n';
    std::cout << "sp: " << start_sp << '\n';
    std::cout << "---------------------------------------------------\n";
  }

  execute();
  runloop.loop();

  auto end_sp = sp - rt_stack.data();
  if (Global::show_vm_state) {
    std::cout << "### end of execution VM state:\n";
    std::cout << "Stack top value: " << sp[-1].description() << '\n';
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
  auto saved_frame_base = frame_base_ptr;

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
        assert(sp[-1].is_float64());
        sp[-1].val.as_float64 = -sp[-1].val.as_float64;
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
        bool bool_val = sp[-1].bool_value();
        sp[-1].set_undefined();
        sp[-1].set_val(!bool_val);
        break;
      }
      case InstType::push:
        exec_push(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::pushi:
        sp[0].set_val(inst.operand.num_float);
        sp += 1;
        break;
      case InstType::push_str:
        exec_push_str(inst.operand.two.opr1, false);
        break;
      case InstType::push_atom:
        exec_push_str(inst.operand.two.opr1, true);
        break;
      case InstType::push_bool:
        sp[0].set_val(bool(inst.operand.two.opr1));
        sp += 1;
        break;
      case InstType::push_this:
        exec_push_this(bool(inst.operand.two.opr1));
        break;
      case InstType::push_null:
        sp[0].tag = JSValue::JS_NULL;
        sp += 1;
        break;
      case InstType::push_undef:
        sp[0].tag = JSValue::UNDEFINED;
        sp += 1;
        break;
      case InstType::pop:
        exec_pop(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::pop_drop:
        sp -= 1;
        sp[0].set_undefined();
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
        assert(frame_base_ptr[inst.operand.two.opr2].tag == JSValue::UNINIT);
        frame_base_ptr[inst.operand.two.opr2].tag = JSValue::UNDEFINED;
        break;
      case InstType::var_dispose:
        exec_var_dispose(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::var_dispose_range:
        exec_var_dispose_range(inst.operand.two.opr1, inst.operand.two.opr2);
        break;
      case InstType::dup_stack_top:
        if (sp[-1].is_RCObject() && sp[-1].val.as_RCObject->get_ref_count() != 0) {
          sp[-1].val.as_RCObject = sp[-1].val.as_RCObject->copy();
        }
        break;
      case InstType::jmp:
        pc = inst.operand.two.opr1;
        break;
      case InstType::jmp_true:
        if (sp[-1].bool_value()) {
          pc = inst.operand.two.opr1;
        }
        break;
      case InstType::jmp_false:
        if (sp[-1].is_falsy()) {
          pc = inst.operand.two.opr1;
        }
        break;
      case InstType::jmp_cond:
        if (sp[-1].bool_value()) {
          pc = inst.operand.two.opr1;
        } else {
          pc = inst.operand.two.opr2;
        }
        break;
      case InstType::je: break;
      case InstType::jne: break;
      case InstType::gt:
      case InstType::lt:
      case InstType::ge:
      case InstType::le:
        exec_comparison(inst.op_type);
        break;
      case InstType::ne: break;
      case InstType::eq: break;
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
      case InstType::ret:
        exec_return();
        if (stop_at_return && frame_base_ptr < saved_frame_base) return CallResult::DONE_NORMAL;
        break;
      case InstType::ret_err: {
        exec_return_error();
        if (stop_at_return && frame_base_ptr < saved_frame_base) return CallResult::DONE_ERROR;
        error_handle();
        break;
      }
      case InstType::fast_add:
        exec_fast_add(inst);
        break;
      case InstType::fast_bin:
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
      case InstType::halt:
        if (!global_end) {
          global_end = true;
          assert(global_sp == sp);
        }
        // return from a task
        else {
          assert(global_sp + 1 == sp);
          sp -= 1;
          sp[0].set_undefined();
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

CallResult NjsVM::call_function(JSFunction *func, const std::vector<JSValue>& args, JSObject *this_obj) {
  prepare_for_call(func, args, this_obj);
  CallResult res = exec_call((int)args.size(), this_obj != nullptr);
  if (res == CallResult::UNFINISHED) {
    res = execute(true);
  }
  return res;
}

void NjsVM::prepare_for_call(JSFunction *func, const std::vector<JSValue>& args, JSObject *this_obj) {
  sp[0].set_val(func);
  sp += 1;
  for (auto& arg : args) {
    sp[0] = arg;
    sp += 1;
  }

  if (this_obj) invoker_this.set_val(this_obj);
}

void NjsVM::pop_drop() {
  sp -= 1;
  sp[0].set_undefined();
}

JSValue NjsVM::peek_stack_top() {
  return sp[-1];
}

void NjsVM::push_stack(JSValue val) {
  sp[0] = val;
  sp += 1;
}

using StackTraceItem = NjsVM::StackTraceItem;

std::vector<StackTraceItem> NjsVM::capture_stack_trace() {
  std::vector<StackTraceItem> trace;

  JSValue *frame_ptr = frame_base_ptr;

  while (frame_ptr != rt_stack_begin) {
    assert(frame_ptr->tag_is(JSValue::STACK_FRAME_META1));
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
    JSValue &val = frame_base_ptr[index];
    return val.tag != JSValue::HEAP_VAL ? val : val.deref_heap();
  }
  if (scope == ScopeType::FUNC_PARAM) {
    assert(frame_base_ptr[1].tag == JSValue::STACK_FRAME_META2);
    JSValue &val = frame_base_ptr[index - (int)func_arg_count];
    return val.tag != JSValue::HEAP_VAL ? val : val.deref_heap();
  }
  if (scope == ScopeType::GLOBAL) {
    JSValue &val = rt_stack_begin[index];
    return val.tag != JSValue::HEAP_VAL ? val : val.deref_heap();
  }
  if (scope == ScopeType::CLOSURE) {
    return function_env()->captured_var[index].deref_heap();
  }
  __builtin_unreachable();
}

JSFunction *NjsVM::function_env() {
  assert(frame_base_ptr[0].tag == JSValue::STACK_FRAME_META1);
  return frame_base_ptr[0].val.as_function;
}

void NjsVM::exec_add() {
  if (sp[-2].is_float64() && sp[-1].is_float64()) {
    sp[-2].val.as_float64 += sp[-1].val.as_float64;
    sp -= 1;
    sp[0].set_undefined();
    return;
  }
  else if (sp[-2].tag_is(JSValue::STRING) && sp[-1].tag_is(JSValue::STRING)) {
    auto *new_str = new PrimitiveString(sp[-2].val.as_primitive_string->str
                                        + sp[-1].val.as_primitive_string->str);
    sp[-2].set_undefined();
    sp[-2].set_val(new_str);
  }
  sp -= 1;
  sp[0].set_undefined();
}

void NjsVM::exec_add_assign(int scope, int index) {
  JSValue& value = get_value(scope_type_from_int(scope), index);
  sp -= 1;
  if (value.is_float64() && sp[0].is_float64()) {
    value.val.as_float64 += sp[0].val.as_float64;
    sp[0].set_undefined();
    return;
  }
  else if (value.tag_is(JSValue::STRING) && sp[0].tag_is(JSValue::STRING)) {
    value.val.as_primitive_string->str += sp[0].val.as_primitive_string->str;
    sp[0].set_undefined();
    return;
  }
  sp[0].set_undefined();
}

void NjsVM::exec_compound_assign(InstType type, int scope, int index) {
  JSValue& value = get_value(scope_type_from_int(scope), index);
  sp -= 1;
  // fast path
  if (value.is_float64() && sp[0].is_float64()) {
    switch (type) {
      case InstType::sub_assign: value.val.as_float64 -= sp[0].val.as_float64; break;
      case InstType::mul_assign: value.val.as_float64 *= sp[0].val.as_float64; break;
      case InstType::div_assign: value.val.as_float64 /= sp[0].val.as_float64; break;
    }
    sp[0].set_undefined();
    return;
  }
  else {
    assert(false);
  }
  sp[0].set_undefined();
}

void NjsVM::exec_inc_or_dec(int scope, int index, int inc) {
  JSValue& value = get_value(scope_type_from_int(scope), index);
  assert(value.is_float64());
  value.val.as_float64 += inc;
}

void NjsVM::exec_binary(InstType op_type) {
  assert(sp[-2].is_float64() && sp[-1].is_float64());
  switch (op_type) {
    case InstType::sub:
      sp[-2].val.as_float64 -= sp[-1].val.as_float64;
      break;
    case InstType::mul:
      sp[-2].val.as_float64 *= sp[-1].val.as_float64;
      break;
    case InstType::div:
      sp[-2].val.as_float64 /= sp[-1].val.as_float64;
      break;
  }
  sp[-1].set_undefined();
  sp -= 1;
}

void NjsVM::exec_logi(InstType op_type) {
  switch (op_type) {
    case InstType::logi_and:
      if (!sp[-2].is_falsy()) {
        sp[-2].set_undefined();
        sp[-2] = std::move(sp[-1]);
      }
      break;
    case InstType::logi_or:
      if (sp[-2].is_falsy()) {
        sp[-2].set_undefined();
        sp[-2] = std::move(sp[-1]);
      }
      break;
  }
  sp -= 1;
}

void NjsVM::exec_fast_add(Instruction& inst) {

  auto lhs_scope = scope_type_from_int(inst.operand.four.opr1);
  auto rhs_scope = scope_type_from_int(inst.operand.four.opr3);
  auto lhs_index = inst.operand.four.opr2;
  auto rhs_index = inst.operand.four.opr4;

  JSValue& lhs_val = get_value(lhs_scope, lhs_index);
  JSValue& rhs_val = get_value(rhs_scope, rhs_index);

  if (lhs_val.is_float64() && rhs_val.is_float64()) {
    sp[0].set_val(lhs_val.val.as_float64 + rhs_val.val.as_float64);
    sp += 1;
  }
  else {
    // currently not support.
    assert(false);
  }
}

void NjsVM::exec_return() {
  JSValue *old_sp = frame_base_ptr - func_arg_count - 1;

  // retain the return value, in case it's deallocated due to the dispose stage bellow.
  JSValue& ret_val = sp[-1];
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->retain();

  // dispose function local storage
  for (JSValue *addr = old_sp + 1; addr < &ret_val; addr++) {
    addr->dispose();
  }
  // but the return value is actually a temporary value, so mark it as temp
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->mark_as_temp();
  // move the return value
  old_sp[0] = std::move(ret_val);

  // restore old state
  sp = old_sp + 1;
  pc = frame_base_ptr[0].flag_bits;
  frame_base_ptr = frame_base_ptr[1].val.as_JSValue;
  func_arg_count = frame_base_ptr[1].flag_bits;
}

void NjsVM::exec_return_error() {
  JSValue *old_sp = frame_base_ptr - func_arg_count - 1;
  JSValue *local_var_end = frame_base_ptr + function_env()->meta.local_var_count + frame_meta_size;

  // retain the return value, in case it's deallocated due to the dispose stage bellow.
  JSValue& ret_val = sp[-1];
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->retain();

  // dispose function local storage
  for (JSValue *addr = old_sp + 1; addr < local_var_end; addr++) {
    addr->dispose();
  }
  // dispose the operand stack
  // TODO: check correctness
  for (JSValue *addr = local_var_end; addr < &ret_val; addr++) {
    addr->set_undefined();
  }
  // but the return value is actually a temporary value, so mark it as temp
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->mark_as_temp();
  // move the return value
  old_sp[0] = std::move(ret_val);

  // restore old state
  sp = old_sp + 1;
  pc = frame_base_ptr[0].flag_bits;
  frame_base_ptr = frame_base_ptr[1].val.as_JSValue;
  func_arg_count = frame_base_ptr[1].flag_bits;
}

void NjsVM::exec_push(int scope, int index) {
  sp[0] = get_value(scope_type_from_int(scope), index);
  sp += 1;
}

void NjsVM::exec_pop(int scope, int index) {
  sp -= 1;
  get_value(scope_type_from_int(scope), index).assign(sp[0]);
  sp[0].set_undefined();
}

void NjsVM::exec_store(int scope, int index) {
  auto var_scope = scope_type_from_int(scope);
  get_value(var_scope, index).assign(sp[-1]);
}

void NjsVM::exec_prop_assign() {
  assert(sp[-1].tag != JSValue::VALUE_HANDLE && sp[-1].tag != JSValue::HEAP_VAL);

  JSValue& target_val = sp[-2];
  if (target_val.tag == JSValue::VALUE_HANDLE) {
    target_val.val.as_JSValue->assign(sp[-1]);
  }
  else {
    printf("VM warning: invalid property assign.\n");
  }

  sp[-1].set_undefined();
  sp[-2].set_undefined();
  sp -= 2;
}

void NjsVM::exec_var_dispose(int scope, int index) {
  auto var_scope = scope_type_from_int(scope);
  assert(var_scope == ScopeType::FUNC || var_scope == ScopeType::GLOBAL);
  JSValue& val = var_scope == ScopeType::FUNC ? frame_base_ptr[index] : rt_stack_begin[index];
  val.dispose();
}

void NjsVM::exec_var_deinit_range(int start, int end) {
  for (int i = start; i < end; i++) {
    frame_base_ptr[i].tag = JSValue::UNINIT;
  }
}

void NjsVM::exec_var_dispose_range(int start, int end) {
  for (int i = start; i < end; i++) {
    frame_base_ptr[i].dispose();
  }
}

void NjsVM::exec_make_func(int meta_idx) {
  auto& meta = func_meta[meta_idx];
  auto *func = new_function(meta);

  // if a function is an arrow function, it captures the `this` value in the environment
  // where the function is created.
  if (meta.is_arrow_func) {
    if (frame_base_ptr != rt_stack_begin) {
      func->This = function_env()->This;
    }
    else {
      func->This = top_level_this;
    }
  }

  if (not meta.is_arrow_func) {
    JSObject *new_prototype = new_object(ObjectClass::CLS_OBJECT);
    new_prototype->add_prop(StringPool::ATOM_constructor, JSValue(func), false);
    func->add_prop(StringPool::ATOM_prototype, JSValue(new_prototype), false);
  }

  if (meta.is_native) {
    assert(!meta.is_anonymous);
    u16string& name = str_pool.get_string(meta.name_index);
    func->native_func = native_func_binding[name];
  }
  sp[0].set_val(func);
  sp += 1;
}

void NjsVM::exec_capture(int scope, int index) {
  auto var_scope = scope_type_from_int(scope);

  assert(sp[-1].tag_is(JSValue::FUNCTION));

  JSFunction& func = *sp[-1].val.as_function;

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
      stack_val = frame_base_ptr + index;
    } else if (var_scope == ScopeType::FUNC_PARAM) {
      stack_val = frame_base_ptr + index - (int)func_arg_count;
    } else if (var_scope == ScopeType::GLOBAL) {
      stack_val = rt_stack_begin + index;
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
  JSValue& func_val = sp[-arg_count - 1];
  assert(func_val.tag == JSValue::FUNCTION);
  assert(func_val.val.as_object->obj_class == ObjectClass::CLS_FUNCTION);
  JSFunction *func = func_val.val.as_function;

  u32 def_param_cnt = func->meta.param_count;
  // If the actually passed arguments are fewer than the formal parameters,
  // fill the vacancy with `undefined`.
  sp += def_param_cnt > arg_count ? (def_param_cnt - arg_count) : 0;
  u32 actual_arg_cnt = std::max(def_param_cnt, u32(arg_count));

  // function parameters are considered `in memory` variables. So retain the RCObjects here.
  for (JSValue *addr = sp - actual_arg_cnt; addr < sp; addr++) {
    if (addr->is_RCObject()) {
      addr->val.as_RCObject->retain();
    }
  }

  if (!(func->meta.is_arrow_func || func->has_this_binding)) {
    // set up the `this` for the function.
    func->This.assign(has_this_object ? invoker_this : global_object);
    invoker_this.set_undefined();
  }

  // note that, even if the function is a native function, we set up the frame metadata
  // because we need it when capture the call stack trace.

  // first cell of a function stack frame: return address and pointer to the function object.
  sp[0].tag = JSValue::STACK_FRAME_META1;
  sp[0].flag_bits = pc;
  sp[0].val.as_function = func;

  // second cell of a function stack frame: saved `frame_base_ptr` and arguments count
  sp[1].tag = JSValue::STACK_FRAME_META2;
  sp[1].flag_bits = actual_arg_cnt;
  sp[1].val.as_JSValue = frame_base_ptr;


  if (not func->meta.is_native) {
    func_arg_count = actual_arg_cnt;
    frame_base_ptr = sp;
    sp = sp + frame_meta_size + func->meta.local_var_count;
    pc = func->meta.code_address;

    return CallResult::UNFINISHED;
  }
  else {
    JSValue *saved_frame_base = frame_base_ptr;
    frame_base_ptr = sp;
    sp += 2;
    JSValue ret = func->native_func(*this, *func, ArrayRef<JSValue>(&func_val + 1, actual_arg_cnt));
    // if the native function throws an error, move this error object to the place where the return value should reside.
    if (ret.tag_is(JSValue::COMP_ERR)) {
      func_val = sp[-1];
      // clean the error object
      sp -= 1;
      sp[0].tag = JSValue::UNDEFINED;
    }
    else {
      func_val = ret;
    }
    // clean the STACK_FRAME_META1 and STACK_FRAME_META2
    sp -= 2;
    sp[0].tag = JSValue::UNDEFINED;
    sp[1].tag = JSValue::UNDEFINED;

    // clean the arguments
    for (JSValue *arg = sp - actual_arg_cnt; arg < sp; arg++) {
      arg->dispose();
    }
    sp -= actual_arg_cnt;
    frame_base_ptr = saved_frame_base;
    return ret.tag_is(JSValue::COMP_ERR) ? CallResult::DONE_ERROR : CallResult::DONE_NORMAL;
  }
}

void NjsVM::exec_js_new(int arg_count) {
  JSValue& ctor = sp[-arg_count - 1];
  assert(ctor.tag_is(JSValue::FUNCTION));

  // prepare `this` object
  JSValue proto = ctor.val.as_function->get_prop(StringPool::ATOM_prototype, false);
  proto = proto.is_object() ? proto : object_prototype;
  JSObject *this_obj = new_object(ObjectClass::CLS_OBJECT, proto);
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

  JSValue& ret_val = sp[-1];
  if (!ret_val.is_object()) {
    ret_val.set_undefined();
    ret_val.set_val(this_obj);
  }
}

void NjsVM::exec_make_object() {
  auto *obj = new_object();
  sp[0].set_val(obj);
  sp += 1;
}

void NjsVM::exec_make_array(int length) {
  auto *array = new_array(length);
  sp[0].set_val(array);
  sp += 1;
}

void NjsVM::exec_fast_assign(Instruction& inst) {

  auto lhs_scope = scope_type_from_int(inst.operand.four.opr1);
  auto rhs_scope = scope_type_from_int(inst.operand.four.opr3);

  JSValue& lhs_val = get_value(lhs_scope, inst.operand.four.opr2);
  JSValue& rhs_val = get_value(rhs_scope, inst.operand.four.opr4);

  lhs_val.assign(rhs_val);
}

void NjsVM::exec_add_props(int props_cnt) {
  JSValue& val_obj = sp[-props_cnt * 2 - 1];
  assert(val_obj.is_object());
  JSObject *object = val_obj.val.as_object;

  for (JSValue *key = sp - props_cnt * 2; key < sp; key += 2) {
    object->add_prop(key[0], key[1]);
    key[0].set_undefined();
    key[1].set_undefined();
  }

  sp = sp - props_cnt * 2;
}

void NjsVM::exec_add_elements(int elements_cnt) {
  JSValue& val_array = sp[-elements_cnt - 1];
  assert(val_array.tag == JSValue::ARRAY);
  JSArray *array = val_array.val.as_array;

  array->dense_array.resize(elements_cnt);

  u32 ele_idx = 0;
  for (JSValue *val = sp - elements_cnt; val < sp; val++, ele_idx++) {
    array->dense_array[ele_idx].assign(*val);
    val[0].set_undefined();
  }

  sp = sp - elements_cnt;
}

void NjsVM::exec_push_str(int str_idx, bool atom) {
  sp[0].tag = atom ? JSValue::JS_ATOM : JSValue::STRING;

  if (atom) {
    sp[0].val.as_int64 = str_idx;
  }
  else {
    auto str = new PrimitiveString(str_pool.get_string(str_idx));
    sp[0].val.as_primitive_string = str;
  }

  sp += 1;
}

void NjsVM::exec_push_this(bool in_global) {
  if (!in_global) {
    assert(function_env());
    sp[0] = function_env()->This;
  }
  else {
    sp[0] = top_level_this;
  }
  sp += 1;
}

void NjsVM::exec_key_access(int key_atom, bool get_ref) {
  JSValue& val_obj = sp[-1];
  invoker_this.assign(val_obj);

  if (Global::show_vm_exec_steps) {
    std::cout << "...visit key " << to_u8string(str_pool.get_string(key_atom)) << '\n';
  }
  if(val_obj.is_object()) {
    if (int64_t(key_atom) == StringPool::ATOM___proto__) {
      val_obj = val_obj.val.as_object->get_prototype();
    } else {
      val_obj = val_obj.val.as_object->get_prop(key_atom, get_ref);
    }
  }
  else if (val_obj.is_undefined() || val_obj.is_null()) {
    goto error;
  }
  else if (!key_access_on_primitive(val_obj, key_atom)) {
    goto error;
  }

  return;
error:
  error_throw(u"cannot read property of " + to_u16string(val_obj.to_string(*this)));
  error_handle();
}

bool NjsVM::key_access_on_primitive(JSValue& obj, int64_t atom) {
  if (obj.is_primitive_string()) {

    obj.set_undefined();
    if (atom == StringPool::ATOM_length) {
      auto len = obj.val.as_primitive_string->length();
      obj.set_val(double(len));
    }
    else if (string_prototype.as_object()->has_own_property(atom)) {
      auto func_val = string_prototype.as_object()->get_prop(atom, false);
      assert(func_val.tag_is(JSValue::FUNCTION));
      obj.set_val(func_val.val.as_function);
    }
  }
  else {
    assert(false);
  }
  return true;
}

void NjsVM::exec_index_access(bool get_ref) {
  JSValue& index = sp[-1];
  JSValue& obj = sp[-2];

  assert(index.tag == JSValue::STRING
        || index.tag == JSValue::JS_ATOM
        || index.tag == JSValue::NUM_FLOAT);
  assert(obj.is_object());

  invoker_this = obj;
  u32 index_int = u32(index.val.as_float64);
  
  // Index an array
  if (obj.tag_is(JSValue::ARRAY)) {
    // The float value can be interpreted as array index
    if (index.is_float64() && index.is_integer() && index.is_non_negative()) {
      obj = obj.val.as_array->access_element(index_int, get_ref);
    }
    // in this case, the float value is interpreted as an ordinary property key
    else if (index.is_float64()) {
      u16string num_str = to_u16string(std::to_string(index.val.as_float64));
      int64_t atom = str_pool.add_string(num_str);
      obj = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.tag_is(JSValue::STRING) || index.tag_is(JSValue::JS_ATOM)) {
      auto& index_str = index.tag_is(JSValue::STRING)
                                    ? index.val.as_primitive_string->str
                                    : str_pool.get_string(index.val.as_int64);

      int64_t idx_int = scan_index_literal(index_str);
      if (idx_int != -1) {
        // string can be converted to number
        obj = obj.val.as_array->access_element(u32(idx_int), get_ref);
      } else {
        // object property
        int64_t atom = index.tag_is(JSValue::STRING) ? str_pool.add_string(index_str)
                                                     : (u32)index.val.as_int64;
        obj = obj.val.as_object->get_prop(atom, get_ref);
      }
    }
  }
  // Index a string
  else if (obj.tag_is(JSValue::STRING)) {
    obj.set_undefined();

    // The float value can be interpreted as string index
    if (index.is_float64() && index.is_integer() && index.is_non_negative()) {
      u16string& str = obj.val.as_primitive_string->str;

      if (index_int < str.size()) {
        auto new_str = new PrimitiveString(u16string(1, str[index_int]));
        obj.set_val(new_str);
      }
    }
    else if (index.tag_is(JSValue::JS_ATOM)) {
      if (index.val.as_int64 == StringPool::ATOM_length) {
        obj.set_val(double(obj.val.as_primitive_string->length()));
      }
    }
  }
  else if (obj.tag_is(JSValue::OBJECT)) {
    if (index.is_float64()) {
      u16string num_str = to_u16string(std::to_string(index.val.as_float64));
      int64_t atom = str_pool.add_string(num_str);
      obj = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.tag_is(JSValue::STRING)) {
      int64_t atom = str_pool.add_string(index.val.as_primitive_string->str);
      obj = obj.val.as_object->get_prop(atom, get_ref);
    }
    else if (index.tag_is(JSValue::JS_ATOM)) {
      obj = obj.val.as_object->get_prop(index.val.as_int64, get_ref);
    }
  }

  index.set_undefined();
  sp -= 1;
}

bool NjsVM::are_strings_equal(const JSValue& lhs, const JSValue& rhs) {
  if (lhs.tag == rhs.tag && lhs.tag_is(JSValue::JS_ATOM)) return true;

  auto get_string_from_value = [this](const JSValue& val) {
    u16string *str_data = nullptr;

    if (val.tag_is(JSValue::JS_ATOM)) {
      str_data = &str_pool.get_string(val.val.as_int64);
    }
    else if (val.tag_is(JSValue::STRING) || val.tag_is(JSValue::STRING_REF)) {
      str_data = &(val.val.as_primitive_string->str);
    }
    else if (val.tag_is(JSValue::STRING_OBJ)) {
      assert(false);
    }
    return str_data;
  };

  u16string *lhs_str = get_string_from_value(lhs);
  u16string *rhs_str = get_string_from_value(rhs);
  
  return *lhs_str == *rhs_str;
}

void NjsVM::exec_strict_equality(bool flip) {
  JSValue& lhs = sp[-2];
  JSValue& rhs = sp[-1];

  if (lhs.tag == rhs.tag) {
    assert(lhs.tag != JSValue::HEAP_VAL && lhs.tag != JSValue::VALUE_HANDLE);
    auto tag = lhs.tag;

    if (tag == JSValue::NUM_FLOAT) {
      lhs.val.as_bool = lhs.val.as_float64 == rhs.val.as_float64;
    }
    else if (tag == JSValue::JS_ATOM || tag == JSValue::NUM_INT) {
      lhs.val.as_bool = lhs.val.as_int64 == rhs.val.as_int64;
    }
    else if (lhs.is_string_type()) {
      bool equal = are_strings_equal(lhs, rhs);
      lhs.set_undefined();
      lhs.val.as_bool = equal;
    }
    else if (lhs.tag_is(JSValue::UNDEFINED) || lhs.tag_is(JSValue::JS_NULL)) {
      lhs.val.as_bool = true;
    }
    else if (lhs.is_object()) {
      lhs.val.as_bool = lhs.val.as_object == rhs.val.as_object;
    }
  }
  else {
    bool equal = false;
    if (lhs.is_string_type() && rhs.is_string_type()) {
      equal = are_strings_equal(lhs, rhs);
    }
    lhs.set_undefined();
    lhs.val.as_bool = equal;
  }

  lhs.tag = JSValue::BOOLEAN;
  if (flip) lhs.val.as_bool = !lhs.val.as_bool;
  rhs.set_undefined();
  sp -= 1;
}

void NjsVM::exec_comparison(InstType type) {
  JSValue& lhs = sp[-2];
  JSValue& rhs = sp[-1];

  bool res = false;
  if (lhs.is_float64() && rhs.is_float64()) {
    switch (type) {
      case InstType::lt: res = lhs.val.as_float64 < rhs.val.as_float64; break;
      case InstType::gt: res = lhs.val.as_float64 > rhs.val.as_float64; break;
      case InstType::le: res = lhs.val.as_float64 <= rhs.val.as_float64; break;
      case InstType::ge: res = lhs.val.as_float64 >= rhs.val.as_float64; break;
    }
  }
  else if (lhs.tag_is(JSValue::STRING) && rhs.tag_is(JSValue::STRING)) {
    switch (type) {
      case InstType::lt: res = *lhs.val.as_primitive_string < *rhs.val.as_primitive_string; break;
      case InstType::gt: res = *lhs.val.as_primitive_string > *rhs.val.as_primitive_string; break;
      case InstType::le: res = *lhs.val.as_primitive_string <= *rhs.val.as_primitive_string; break;
      case InstType::ge: res = *lhs.val.as_primitive_string >= *rhs.val.as_primitive_string; break;
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
  JSValue err_obj = InternalFunctions::error_build_internal(*this, msg);
  sp[0] = err_obj;
  sp += 1;
}

void NjsVM::error_handle() {
  // 1. error happens in a function (or a task)
  // if the error does not happen in a task, the error position should be (pc - 1)
  u32 err_throw_pc = pc - u32(!global_end);
  auto& catch_table = frame_base_ptr != rt_stack_begin ? function_env()->meta.catch_table
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
      if (frame_base_ptr == rt_stack_begin) sp_restore = global_sp;
      else sp_restore = frame_base_ptr + function_env()->meta.local_var_count + frame_meta_size;

      // dispose the operand stack if needed
      if (sp - 1 != sp_restore) {
        for (JSValue *val = sp_restore; val < sp - 1; val++) {
          val->set_undefined();
        }
        *sp_restore = std::move(sp[-1]);
        sp = sp_restore + 1;
      }

      // dispose local variables
      JSValue *local_start = frame_base_ptr + entry.local_var_begin;
      for (JSValue *val = local_start; val < frame_base_ptr + entry.local_var_end; val++) {
        val->dispose();
      }

      break;
    }
  }
  // 1.2 an error happens but there is no `catch`
  if (!catch_entry) {
    assert(catch_table[catch_table.size() - 1].start_pos == catch_table[catch_table.size() - 1].end_pos);
    pc = catch_table[catch_table.size() - 1].goto_pos;
  }
}

void NjsVM::exec_halt_err(Instruction &inst) {
  if (!global_end) global_end = true;
  assert(sp > global_sp);

  JSValue err_val = sp[-1];

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
  for (JSValue *val = global_sp; val < sp; val++) {
    val->set_undefined();
  }
  sp = global_sp;
  if (Global::show_vm_exec_steps) {
    printf("\033[33m%-50s sp: %-3ld   pc: %-3u\033[0m\n\n", inst.description().c_str(), sp - rt_stack.data(), pc);
  }
}

}