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
#include "njs/basic_types/JSErrorPrototype.h"

#define TRY(expression)                                                                       \
    ({                                                                                        \
        Completion _temp_result = (expression);                                               \
        if (_temp_result.is_throw()) [[unlikely]] {                                           \
          *(++sp) = _temp_result.get_value();                                                 \
          error_handle(sp);                                                                   \
          return;                                                                             \
        }                                                                                     \
        _temp_result.get_value();                                                             \
    })

namespace njs {

NjsVM::NjsVM(CodegenVisitor& visitor)
  : heap(600, *this)
  , bytecode(std::move(visitor.bytecode))
  , runloop(*this)
  , atom_pool(std::move(visitor.atom_pool))
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
      global_obj->set_prop(*this, sym_name, undefined);
    }
  }

  global_meta.name_index = atom_pool.atomize(u"(global)");
  global_meta.is_strict = global_scope.is_strict;
  global_meta.local_var_count = global_scope.get_var_count();
  global_meta.stack_size = global_scope.get_max_stack_size();
  global_meta.param_count = 0;
  global_meta.code_address = 0;
  global_meta.source_line = 0;

  atom_pool.record_static_atom_count();
}

NjsVM::~NjsVM() {
  assert(curr_frame != nullptr);
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

  native_error_protos.reserve(JSErrorType::JS_NATIVE_ERROR_COUNT);
  for (int i = 0; i < JSErrorType::JS_NATIVE_ERROR_COUNT; i++) {
    JSObject *proto = heap.new_object<JSErrorPrototype>(*this, (JSErrorType)i);
    proto->set_proto(object_prototype);
    native_error_protos.emplace_back(proto);
  }
  error_prototype = native_error_protos[0];
}

JSObject* NjsVM::new_object(ObjClass cls) {
  return heap.new_object<JSObject>(cls, object_prototype);
}

JSObject* NjsVM::new_object(ObjClass cls, JSValue proto) {
  return heap.new_object<JSObject>(cls, proto);
}

JSFunction* NjsVM::new_function(const JSFunctionMeta& meta) {
  JSFunction *func;
  if (meta.is_anonymous) {
    func = heap.new_object<JSFunction>(meta);
  } else {
    func = heap.new_object<JSFunction>(atom_to_str(meta.name_index), meta);
  }
  func->set_proto(function_prototype);
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

//  if (!log_buffer.empty()) {
//    std::cout << "------------------------------" << std::endl << "log:" << std::endl;
//    for (auto& str: log_buffer) {
//      std::cout << str;
//    }
//  }

}

void NjsVM::execute_global() {
  auto *func = heap.new_object<JSFunction>(global_meta);
  call_internal(func, global_object, nullptr, ArrayRef<JSValue>(nullptr, 0), CallFlags());
}

Completion NjsVM::call_internal(JSFunction *callee, JSValue This,
                                JSFunction *new_target, ArrayRef<JSValue> argv, CallFlags flags) {
  // setup call stack
  JSStackFrame frame;
  frame.prev_frame = curr_frame;
  frame.function = JSValue(callee);

  stack_frames.push_back(&frame);
  curr_frame = &frame;

  bool copy_argv = (argv.size() < callee->meta.param_count) | flags.copy_args;
  size_t actual_arg_cnt = std::max(argv.size(), (size_t)callee->meta.param_count);
  size_t args_buf_cnt = unlikely(copy_argv) ? actual_arg_cnt : 0;
  size_t alloc_cnt = args_buf_cnt + frame_meta_size +
                     callee->meta.local_var_count + callee->meta.stack_size;

  JSValue *buffer = (JSValue *)alloca(sizeof(JSValue) * alloc_cnt);
  JSValue *args_buf = unlikely(copy_argv) ? buffer : argv.data();

  if (copy_argv) [[unlikely]] {
    memcpy(args_buf, argv.data(), sizeof(JSValue) * argv.size());
    for (JSValue *val = args_buf + argv.size(); val < args_buf + args_buf_cnt; val++) {
      val->set_undefined();
    }
  }

  if (callee->meta.is_native) {
    Completion comp;
    ArrayRef<JSValue> args_ref(args_buf, actual_arg_cnt);
    if (new_target != nullptr) [[unlikely]] {
      flags.this_is_new_target = true;
      comp = callee->native_func(*this, *callee, JSValue(new_target), args_ref, flags);
    } else {
      comp = callee->native_func(*this, *callee, This, args_ref, flags);
    }
    curr_frame = curr_frame->prev_frame;
    stack_frames.pop_back();
    return comp;
  }

  JSValue *local_vars = buffer + args_buf_cnt;
  JSValue *stack = local_vars + callee->meta.local_var_count + frame_meta_size;
  // Initialize local variables and operation stack to undefined
  for (JSValue *val = local_vars; val < buffer + alloc_cnt; val++) {
    val->set_undefined();
  }

  JSValue *sp = stack - 1;
  u32 pc = callee->meta.code_address;

  frame.alloc_cnt = alloc_cnt;
  frame.buffer = buffer;
  frame.args_buf = args_buf;
  frame.local_vars = local_vars;
  frame.stack = stack;
  frame.pc_ref = &pc;

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
    JSFunction& func = *sp[0].val.as_func;

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
        if (val.is_uninited()) [[unlikely]] {
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
        sp[0].val.as_prim_string =
            heap.new_object<PrimitiveString>(atom_to_str(OPR1));
        break;
      case OpType::push_atom:
        sp += 1;
        sp[0].tag = JSValue::JS_ATOM;
        sp[0].val.as_atom = OPR1;
        break;
      case OpType::push_bool:
        sp += 1;
        sp[0].set_val(bool(OPR1));
        break;
      case OpType::push_func_this:
        assert(callee);
        sp += 1;
        sp[0] = This;
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
        if (val.is_uninited()) [[unlikely]] {
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
        if (val.is_uninited()) [[unlikely]] {
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
        exec_call(sp, OPR1, bool(OPR2), nullptr);
        break;
      }
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
      case OpType::get_prop_atom2: {
        int keep_obj = static_cast<int>(inst.op_type) - static_cast<int>(OpType::get_prop_atom);
        exec_get_prop_atom(sp, OPR1, keep_obj);
        break;
      }
      case OpType::get_prop_index:
      case OpType::get_prop_index2: {
        int keep_obj = static_cast<int>(inst.op_type) - static_cast<int>(OpType::get_prop_index);
        exec_get_prop_index(sp, keep_obj);
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
  auto comp = call_function(
      task.task_func.val.as_func,
      global_object,
      nullptr,
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

Completion NjsVM::call_function(JSFunction *func, JSValue This, JSFunction *new_target,
                                const vector<JSValue>& args, CallFlags flags) {
  JSValue& this_obj = This.is_undefined() ? global_object : This;
  JSValue& actual_this = func->has_this_binding ? func->this_binding : this_obj;
  ArrayRef<JSValue> args_ref(const_cast<JSValue*>(args.data()), args.size());
  return call_internal(func, actual_this, new_target, args_ref, flags);
}

using StackTraceItem = NjsVM::StackTraceItem;

vector<StackTraceItem> NjsVM::capture_stack_trace() {
  vector<StackTraceItem> trace;

  for (size_t i = stack_frames.size() - 1; i > 0; i--) {
    auto func = stack_frames[i]->function.val.as_func;
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

JSValue NjsVM::build_error_internal(const u16string& msg) {
  return build_error_internal(JS_ERROR, msg);
}

JSValue NjsVM::build_error_internal(JSErrorType type, const u16string& msg) {
  auto *err_obj = new_object(ObjClass::CLS_ERROR, native_error_protos[type]);
  err_obj->set_prop(*this, u"message", new_primitive_string(msg));

  u16string trace_str = build_trace_str();
  err_obj->set_prop(*this, u"stack", new_primitive_string(std::move(trace_str)));

  return JSValue(err_obj);
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
        sp[-1].val.as_prim_string->str + sp[0].val.as_prim_string->str
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
    func->this_binding = env_this;
  }

  if (not meta.is_arrow_func) {
    JSObject *new_prototype = new_object(ObjClass::CLS_OBJECT);
    PropFlag flag {.configurable = true, .writable = true, .has_value = true};
    new_prototype->set_prop(*this, AtomPool::k_constructor, JSValue(func), flag);
    func->set_prop(*this, AtomPool::k_prototype, JSValue(new_prototype), flag);
  }

  assert(!meta.is_native);

  sp += 1;
  sp[0].set_val(func);
}


CallResult NjsVM::exec_call(SPRef sp, int arg_count, bool has_this_object, JSFunction *new_target) {
  JSValue& func_val = sp[-arg_count];
  assert(func_val.tag == JSValue::FUNCTION);
  assert(func_val.val.as_object->get_class() == ObjClass::CLS_FUNCTION);
  JSFunction *func = func_val.val.as_func;

  JSValue& this_obj = has_this_object ? sp[-arg_count - 1] : global_object;
  JSValue& actual_this = func->has_this_binding ? func->this_binding : this_obj;
  JSValue *argv = &func_val + 1;
  ArrayRef<JSValue> args_ref(argv, arg_count);
  // call the function
  Completion comp = call_internal(func, actual_this, new_target, args_ref, CallFlags());
  // clear the arguments
  for (JSValue *val = argv; val < argv + arg_count; val++) {
    val->set_undefined();
  }
  // put the return value to the correct place
  if (has_this_object) {
    this_obj = comp.get_value();
    func_val.set_undefined();
  } else {
    func_val = comp.get_value();
  }
  sp -= (arg_count + int(has_this_object));

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
  JSValue proto = ctor.val.as_func->get_prop_trivial(AtomPool::k_prototype);
  proto = proto.is_object() ? proto : object_prototype;
  auto *this_obj = heap.new_object<JSObject>(ObjClass::CLS_OBJECT, proto);

  for (int i = 1; i > -arg_count; i--) {
    sp[i] = sp[i - 1];
  }
  sp[-arg_count].set_val(this_obj);
  sp += 1;

  // run the constructor
  CallResult res = exec_call(sp, arg_count, true, ctor.val.as_func);
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
    assert(key[0].is_atom());
    object->add_prop_trivial(key[0].val.as_atom, key[1]);
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


void NjsVM::exec_get_prop_atom(SPRef sp, u32 key_atom, int keep_obj) {
  JSValue& val_obj = sp[0];

  if (Global::show_vm_exec_steps) {
    std::cout << "...visit key " << to_u8string(atom_to_str(key_atom)) << '\n';
  }
  bool is_throw = false;
  if(val_obj.is_object()) {
    if (key_atom == AtomPool::k___proto__) [[unlikely]] {
      sp[keep_obj] = val_obj.val.as_object->get_proto();
    } else {
      auto comp = val_obj.val.as_object->get_prop(*this, key_atom);
      is_throw = comp.is_throw();
      sp[keep_obj] = comp.get_value();
    }
  } else if (not (val_obj.is_uninited() || val_obj.is_undefined() || val_obj.is_null())) {
    auto comp = get_prop_on_primitive(val_obj, key_atom);
    is_throw = comp.is_throw();
    sp[keep_obj] = comp.get_value();
  } else {
    error_throw(sp, u"cannot read property of " + to_u16string(val_obj.to_string(*this)));
    error_handle(sp);
    return;
  }

  sp += keep_obj;
  if (is_throw) [[unlikely]] {
    error_handle(sp);
  } else {
    if (sp[keep_obj].is_uninited()) val_obj = undefined;
  }
}

Completion NjsVM::get_prop_on_primitive(JSValue& obj, u32 atom) {
  switch (obj.tag) {
    case JSValue::BOOLEAN:
    case JSValue::NUM_FLOAT:
    case JSValue::SYMBOL:
      return undefined;
      break;
    case JSValue::STRING:
      if (atom == AtomPool::k_length) {
        auto len = obj.val.as_prim_string->length();
        return JSValue(double(len));
      } else {
        return string_prototype.as_object()->get_prop(*this, atom);
      }
    default:
      assert(false);
  }
}

void NjsVM::exec_get_prop_index(SPRef sp, int keep_obj) {
  int res_index = keep_obj - 1;
  JSValue& index = sp[0];
  JSValue& obj = sp[-1];

  Completion comp = get_at_index(obj, index);

  sp[res_index].set_undefined();
  index.set_undefined();
  sp[res_index] = comp.get_value();
  sp += res_index;

  if (comp.is_throw()) [[unlikely]] {
    error_handle(sp);
  }
}

Completion NjsVM::get_at_index(JSValue obj, JSValue index) {
  // Index an array
  if (obj.is(JSValue::ARRAY)) {
    return obj.val.as_array->get_element(*this, index);
  }
  // Index a string
  else if (obj.is_prim_string()) {
    auto *str = obj.val.as_prim_string;
    Completion comp = to_property_key(*this, index);
    if (comp.is_throw()) return comp;
    u32 atom = comp.get_value().val.as_atom;
    if (atom_is_int(atom)) {
      u32 idx = atom_get_int(atom);
      if (idx < str->length()) {
        return JSValue(new_primitive_string(u16string(1, str->str[idx])));
      } else {
        return undefined;
      }
    } else {
      return string_prototype.as_object()->get_prop(*this, atom);
    }
  }
  else if (obj.is_object()) {
    Completion comp = to_property_key(*this, index);
    if (comp.is_throw()) return comp;
    return obj.as_object()->get_prop(*this, comp.get_value());
  }
  assert(false);
}

Completion NjsVM::set_at_index(JSValue obj, JSValue index, JSValue val) {
#define RET_ERR_OR_UNDEF { if (res.is_error()) { return Completion::with_throw(res.get_error()); } return undefined; }

  if (obj.is(JSValue::ARRAY)) {
    return obj.val.as_array->set_element(*this, index, val);
  }
  else if (obj.is_object()) {
    Completion comp = to_property_key(*this, index);
    if (comp.is_throw()) return comp;
    auto res = obj.val.as_object->set_prop(*this, comp.get_value(), val);
    RET_ERR_OR_UNDEF
  }
  assert(false);
}

// top of stack: value, obj
void NjsVM::exec_set_prop_atom(SPRef sp, u32 key_atom) {
  JSValue &val = sp[0];
  JSValue &obj = sp[-1];

  if (Global::show_vm_exec_steps) {
    std::cout << "...visit key " << to_u8string(atom_to_str(key_atom)) << '\n';
  }
  if (obj.is_object()) {
    if (key_atom == AtomPool::k___proto__) [[unlikely]] {
      obj.val.as_object->set_proto(val);
    } else {
      auto res = obj.val.as_object->set_prop(*this, key_atom, val);
      if (res.is_error()) {
        sp += 1;
        sp[0] = res.get_error();
        error_handle(sp);
        return;
      }
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
    Completion comp = set_at_index(obj, index, val);
    if (comp.is_throw()) [[unlikely]] {
      *++sp = comp.get_value();
      error_handle(sp);
      return;
    }
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
  JSValue val = global_object.as_object()->get_prop_trivial(name_atom);

  if (val.is_uninited()) {
    u16string var_name = atom_to_str(name_atom);
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
      case OpType::lt: res = *lhs.val.as_prim_string < *rhs.val.as_prim_string; break;
      case OpType::gt: res = *lhs.val.as_prim_string > *rhs.val.as_prim_string; break;
      case OpType::le: res = *lhs.val.as_prim_string <= *rhs.val.as_prim_string; break;
      case OpType::ge: res = *lhs.val.as_prim_string >= *rhs.val.as_prim_string; break;
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
  JSValue err_obj = build_error_internal(msg);
  sp += 1;
  sp[0] = err_obj;
}

void NjsVM::error_handle(SPRef sp) {
  bool in_global = curr_frame->prev_frame == nullptr;
  auto this_func = curr_frame->function.val.as_func;
  auto frame = curr_frame;
  u32& pc = *frame->pc_ref;

  // TODO: check this
  u32 err_throw_pc = pc - 1;
  auto& catch_table = unlikely(in_global) ? global_catch_table
                                          : this_func->meta.catch_table;
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

void NjsVM::print_unhandled_error(JSValue err) {
  if (err.is_object() && err.as_object()->get_class() == ObjClass::CLS_ERROR) {
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