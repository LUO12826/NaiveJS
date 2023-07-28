#include "NjsVM.h"

#include <iostream>
#include "njs/basic_types/JSArray.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/global_var.h"

namespace njs {

NjsVM::NjsVM(CodegenVisitor& visitor) : heap(600, *this)
                                      , rt_stack(max_stack_size)
                                      , bytecode(std::move(visitor.bytecode))
                                      , runloop(*this)
                                      , str_pool(std::move(visitor.str_pool))
                                      , num_list(std::move(visitor.num_list))
                                      , func_meta(std::move(visitor.func_meta))
                                      , top_level_this(heap.new_object<JSObject>())
                                      , global_object(heap.new_object<GlobalObject>())
{
  auto& global_sym_table = visitor.scope_chain[0]->get_symbol_table();

  GlobalObject& global_obj = *static_cast<GlobalObject *>(global_object.val.as_object);
  for (auto& [sym_name, sym_rec] : global_sym_table) {
    global_obj.props_index_map.emplace(u16string(sym_name), sym_rec.index + frame_meta_size);
  }

  sp = global_sym_table.size() + frame_meta_size;
}

void NjsVM::add_native_func_impl(u16string name, NativeFuncType func) {
  native_func_binding.emplace(std::move(name), func);
}

void NjsVM::add_builtin_object(u16string name,
                               std::function<JSObject*(GCHeap&, StringPool&)> builder) {
  GlobalObject& global_obj = *static_cast<GlobalObject *>(global_object.val.as_object);
  auto iter = global_obj.props_index_map.find(name);
  if (iter == global_obj.props_index_map.end()) return;

  u32 index = iter->second;
  rt_stack[index] = JSValue(builder(heap, str_pool));
}

void NjsVM::run() {
  std::cout << "### VM starts execution" << std::endl;

  execute();
  runloop.loop();

  std::cout << "### end of execution VM state: " << std::endl;
  std::cout << rt_stack[sp - 1].description() << std::endl << std::endl;

//  if (!log_buffer.empty()) {
//    std::cout << "------------------------------" << std::endl << "log:" << std::endl;
//    for (auto& str: log_buffer) {
//      std::cout << str;
//    }
//  }

}

void NjsVM::execute() {
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
        assert(rt_stack[sp - 1].tag == JSValue::NUM_FLOAT);
        rt_stack[sp - 1].val.as_float64 = -rt_stack[sp - 1].val.as_float64;
        break;
      case InstType::logi_and:
      case InstType::logi_or:
        exec_logi(inst.op_type);
        break;
      case InstType::logi_not: {
        bool bool_val = rt_stack[sp - 1].bool_value();
        rt_stack[sp - 1].set_undefined();
        rt_stack[sp - 1] = JSValue(!bool_val);
        break;
      }
      case InstType::push:
        exec_push(inst);
        break;
      case InstType::pushi:
        rt_stack[sp] = JSValue(inst.operand.num_float);
        sp += 1;
        break;
      case InstType::push_str:
        exec_push_str(inst.operand.two.opr1, false);
        break;
      case InstType::push_atom:
        exec_push_str(inst.operand.two.opr1, true);
        break;
      case InstType::push_bool:
        rt_stack[sp] = JSValue(bool(inst.operand.two.opr1));
        sp += 1;
        break;
      case InstType::push_this:
        exec_push_this();
        break;
      case InstType::push_null:
        rt_stack[sp].tag = JSValue::JS_NULL;
        sp += 1;
        break;
      case InstType::push_undef:
        rt_stack[sp].tag = JSValue::UNDEFINED;
        sp += 1;
        break;
      case InstType::pop:
        exec_pop(inst);
        break;
      case InstType::pop_drop:
        sp -= 1;
        rt_stack[sp].set_undefined();
        break;
      case InstType::store:
        exec_store(inst);
        break;
      case InstType::prop_assign:
        exec_prop_assign();
        break;
      case InstType::jmp:
        pc = inst.operand.two.opr1;
        break;
      case InstType::jmp_true:
        if (rt_stack[sp - 1].bool_value()) {
          pc = inst.operand.two.opr1;
        }
        break;
      case InstType::jmp_false:
        if (rt_stack[sp - 1].is_falsy()) {
          pc = inst.operand.two.opr1;
        }
        break;
      case InstType::jmp_cond:
        if (rt_stack[sp - 1].bool_value()) {
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
      case InstType::call:
        exec_call(inst.operand.two.opr1, bool(inst.operand.two.opr2));
        break;
      case InstType::ret:
        exec_return();
        if (pc == bytecode.size()) {
          sp -= 1;
          rt_stack[sp].set_undefined();
          return;
        }
        break;
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
        exec_capture(inst);
        break;
      case InstType::make_obj:
        exec_make_object();
        break;
      case InstType::make_array:
        exec_make_array();
        break;
      case InstType::add_props:
        exec_add_props(inst.operand.two.opr1);
        break;
      case InstType::add_elements:
        exec_add_elements(inst.operand.two.opr1);
        break;
      case InstType::halt:
        return;
      case InstType::nop:
        break;
      case InstType::keypath_access:
        exec_keypath_access(inst.operand.two.opr1, (bool)inst.operand.two.opr2);
        break;
      case InstType::index_access:
        exec_index_access((bool)inst.operand.two.opr1);
        break;
    }
    if (Global::show_vm_exec_steps) {
      printf("%-50s sp: %-3u   pc: %-3u\n", inst.description().c_str(), sp, pc);
    }
  }
}

void NjsVM::execute_task(JSTask& task) {
  rt_stack[sp] = task.task_func;
  sp += 1;
  exec_call(0, false);
  execute();
}

u32 NjsVM::calc_var_addr(ScopeType scope, int raw_index) {
  if (scope == ScopeType::GLOBAL) return 0 + frame_meta_size + raw_index;
  if (scope == ScopeType::FUNC) return frame_base_ptr + frame_meta_size + raw_index;
  if (scope == ScopeType::FUNC_PARAM) {
    assert(rt_stack[frame_base_ptr + 1].tag == JSValue::STACK_FRAME_META2);
    u32 arg_count = rt_stack[frame_base_ptr + 1].flag_bits;
    return frame_base_ptr - arg_count + raw_index;
  }
  if (scope == ScopeType::CLOSURE) return raw_index;
  __builtin_unreachable();
}

JSFunction *NjsVM::function_env() {
  if (frame_base_ptr == 0) return nullptr;
  assert(rt_stack[frame_base_ptr].tag == JSValue::STACK_FRAME_META1);
  return rt_stack[frame_base_ptr].val.as_function;
}

void NjsVM::exec_add() {
  if (rt_stack[sp - 2].tag_is(JSValue::NUM_FLOAT) && rt_stack[sp - 1].tag_is(JSValue::NUM_FLOAT)) {
    rt_stack[sp - 2].val.as_float64 += rt_stack[sp - 1].val.as_float64;
  }
  else if (rt_stack[sp - 2].tag_is(JSValue::STRING) && rt_stack[sp - 1].tag_is(JSValue::STRING)) {
    auto *new_str = new PrimitiveString(rt_stack[sp - 2].val.as_primitive_string->str
                                        + rt_stack[sp - 1].val.as_primitive_string->str);
    rt_stack[sp - 2].set_undefined();
    rt_stack[sp - 2] = JSValue(new_str);
  }
  sp -= 1;
  rt_stack[sp].set_undefined();
}

void NjsVM::exec_binary(InstType op_type) {
  assert(rt_stack[sp - 2].tag == JSValue::NUM_FLOAT && rt_stack[sp - 1].tag == JSValue::NUM_FLOAT);
  switch (op_type) {
    case InstType::sub:
      rt_stack[sp - 2].val.as_float64 -= rt_stack[sp - 1].val.as_float64;
      break;
    case InstType::mul:
      rt_stack[sp - 2].val.as_float64 *= rt_stack[sp - 1].val.as_float64;
      break;
    case InstType::div:
      rt_stack[sp - 2].val.as_float64 /= rt_stack[sp - 1].val.as_float64;
      break;
  }
  rt_stack[sp - 1].set_undefined();
  sp -= 1;
}

void NjsVM::exec_logi(InstType op_type) {
  switch (op_type) {
    case InstType::logi_and:
      if (!rt_stack[sp - 2].is_falsy()) {
        rt_stack[sp - 2].set_undefined();
        rt_stack[sp - 2] = std::move(rt_stack[sp - 1]);
      }
      break;
    case InstType::logi_or:
      if (rt_stack[sp - 2].is_falsy()) {
        rt_stack[sp - 2].set_undefined();
        rt_stack[sp - 2] = std::move(rt_stack[sp - 1]);
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

  JSValue& lhs_val = lhs_scope == ScopeType::CLOSURE ?
                                  function_env()->captured_var[lhs_index].deref():
                                  rt_stack[calc_var_addr(lhs_scope, lhs_index)];
  JSValue& rhs_val = rhs_scope == ScopeType::CLOSURE ?
                                  function_env()->captured_var[rhs_index].deref():
                                  rt_stack[calc_var_addr(rhs_scope, rhs_index)];

  if (lhs_val.tag == JSValue::NUM_FLOAT && rhs_val.tag == JSValue::NUM_FLOAT) {
    rt_stack[sp] = JSValue(lhs_val.val.as_float64 + rhs_val.val.as_float64);
    sp += 1;
  }
  else {
    // currently not support.
    assert(false);
  }
}

void NjsVM::exec_return() {
  invoker_this.tag = JSValue::UNDEFINED;
  rt_stack[frame_base_ptr].val.as_function->This.tag = JSValue::UNDEFINED;

  u32 old_frame_bottom = rt_stack[frame_base_ptr + 1].val.as_int64;
  u32 arg_cnt = rt_stack[frame_base_ptr + 1].flag_bits;

  u32 old_sp = frame_base_ptr - arg_cnt - 1;
  u32 old_pc = rt_stack[frame_base_ptr].flag_bits;

  JSValue& ret_val = rt_stack[sp - 1];
  if (ret_val.is_RCObject()) ret_val.val.as_RCObject->retain();

  for (u32 addr = old_sp + 1; addr < sp - 1; addr++) {
    rt_stack[addr].dispose();
  }

  // restore old state
  pc = old_pc;
  sp = old_sp;
  frame_base_ptr = old_frame_bottom;

  // move return value
  rt_stack[old_sp] = std::move(ret_val);
  sp += 1;
}

void NjsVM::exec_push(Instruction& inst) {
  auto var_scope = scope_type_from_int(inst.operand.two.opr1);
  if (var_scope == ScopeType::CLOSURE) {
    JSValue& val = function_env()->captured_var[inst.operand.two.opr2];
    rt_stack[sp] = val.deref();
  }
  else {
    JSValue& val = rt_stack[calc_var_addr(var_scope, inst.operand.two.opr2)];
    rt_stack[sp] = val.tag_is(JSValue::HEAP_VAL) ? val.deref() : val;
  }
  sp += 1;
}

void NjsVM::exec_pop(Instruction& inst) {
  auto var_scope = scope_type_from_int(inst.operand.two.opr1);
  u32 var_addr = calc_var_addr(var_scope, inst.operand.two.opr2);

  sp -= 1;
  if (var_scope == ScopeType::CLOSURE) {
    assert(function_env());
    JSValue& closure_val = function_env()->captured_var[var_addr];
    closure_val.deref().assign(rt_stack[sp]);
  }
  else {
    JSValue& val = rt_stack[var_addr].tag == JSValue::HEAP_VAL ?
                       rt_stack[var_addr].deref() : rt_stack[var_addr];
    val.assign(rt_stack[sp]);
  }

  rt_stack[sp].set_undefined();
}

void NjsVM::exec_store(Instruction& inst) {
  auto var_scope = scope_type_from_int(inst.operand.two.opr1);
  u32 var_addr = calc_var_addr(var_scope, inst.operand.two.opr2);

  if (var_scope == ScopeType::CLOSURE) {
    assert(function_env());
    JSValue& closure_val = function_env()->captured_var[var_addr];
    closure_val.deref().assign(rt_stack[sp - 1]);
  }
  else {
    JSValue& val = rt_stack[var_addr].tag == JSValue::HEAP_VAL ?
                       rt_stack[var_addr].deref() : rt_stack[var_addr];
    val.assign(rt_stack[sp - 1]);
  }
}

void NjsVM::exec_prop_assign() {
  JSValue& target_val = rt_stack[sp - 2];
  assert(target_val.tag == JSValue::VALUE_HANDLE);
  target_val.deref().assign(rt_stack[sp - 1].deref_if_needed());

  rt_stack[sp - 1].set_undefined();
  rt_stack[sp - 2].set_undefined();
  sp -= 2;
}

void NjsVM::exec_make_func(int meta_idx) {
  auto& meta = func_meta[meta_idx];
  const u16string& name = meta.is_anonymous ? u"" : str_pool.get_list()[meta.name_index];

  auto *func = heap.new_object<JSFunction>(name, meta);
  if (meta.is_native) {
    func->native_func = native_func_binding[name];
  }
  rt_stack[sp] = JSValue(func);
  sp += 1;
}

void NjsVM::exec_capture(Instruction& inst) {
  assert(rt_stack[sp - 1].is_object());

  auto var_scope = scope_type_from_int(inst.operand.two.opr1);
  JSFunction& func = *rt_stack[sp - 1].val.as_function;

  if (var_scope == ScopeType::CLOSURE) {
    JSFunction *env_func = function_env();
    assert(env_func);
    JSValue& closure_val = env_func->captured_var[inst.operand.two.opr2];
    closure_val.val.as_heap_val->retain();
    func.captured_var.push_back(closure_val);
  }
  else {
    JSValue& stack_val = rt_stack[calc_var_addr(var_scope, inst.operand.two.opr2)];
    stack_val.move_to_heap();
    stack_val.val.as_heap_val->retain();
    func.captured_var.push_back(stack_val);
  }
}

void NjsVM::exec_call(int arg_count, bool has_this_object) {
  JSValue& func_val = rt_stack[sp - arg_count - 1];
  assert(func_val.tag == JSValue::FUNCTION);
  assert(func_val.val.as_object->obj_class == ObjectClass::CLS_FUNCTION);
  JSFunction *func = func_val.val.as_function;

  u32 def_param_cnt = func->param_count;
  // If the actually passed arguments are fewer than the formal parameters,
  // fill the vacancy with `undefined`.
  sp += def_param_cnt > arg_count ? (def_param_cnt - arg_count) : 0;
  u32 actual_arg_cnt = std::max(def_param_cnt, u32(arg_count));

  // function parameters are considered `in memory` variables. So retain the RCObjects here.
  for (u32 addr = sp - actual_arg_cnt; addr < sp; addr++) {
    if (rt_stack[addr].is_RCObject()) {
      rt_stack[addr].val.as_RCObject->retain();
    }
  }

  // set up the `this` for the function.
  func->This = has_this_object ? invoker_this : JSValue(&global_object);

  if (func->is_native) {
    func_val = func->native_func(*this, *func, ArrayRef<JSValue>(&func_val + 1, actual_arg_cnt));
    for (u32 addr = sp - actual_arg_cnt; addr < sp; addr++) {
      rt_stack[addr].dispose();
    }
    sp -= actual_arg_cnt;
    return;
  }

  // first cell of a function stack frame: return address and pointer to the function object.
  rt_stack[sp].tag = JSValue::STACK_FRAME_META1;
  rt_stack[sp].flag_bits = pc;
  rt_stack[sp].val.as_function = func;

  // second cell of a function stack frame: saved `frame_base_ptr` and arguments count
  rt_stack[sp + 1].tag = JSValue::STACK_FRAME_META2;
  rt_stack[sp + 1].flag_bits = actual_arg_cnt;
  rt_stack[sp + 1].val.as_int64 = frame_base_ptr;

  frame_base_ptr = sp;
  sp = frame_base_ptr + 2 + func->local_var_count;
  pc = func->code_address;
}

void NjsVM::exec_make_object() {
  auto *obj = heap.new_object<JSObject>();
  rt_stack[sp] = JSValue(obj);
  sp += 1;
}

void NjsVM::exec_make_array() {
  auto *array = heap.new_object<JSArray>();
  rt_stack[sp] = JSValue(array);
  sp += 1;
}

void NjsVM::exec_fast_assign(Instruction& inst) {

  auto lhs_scope = scope_type_from_int(inst.operand.four.opr1);
  auto rhs_scope = scope_type_from_int(inst.operand.four.opr3);

  JSValue& lhs_val = rt_stack[calc_var_addr(lhs_scope, inst.operand.four.opr2)].deref_if_needed();
  JSValue& rhs_val = rt_stack[calc_var_addr(rhs_scope, inst.operand.four.opr4)].deref_if_needed();

  lhs_val.assign(rhs_val);
}

void NjsVM::exec_add_props(int props_cnt) {
  JSValue& val_obj = rt_stack[sp - props_cnt * 2 - 1];
  assert(val_obj.is_object());
  JSObject *object = val_obj.val.as_object;

  for (u32 i = sp - props_cnt * 2; i < sp; i += 2) {
    object->add_prop(rt_stack[i], rt_stack[i + 1]);
    rt_stack[i].set_undefined();
    rt_stack[i + 1].set_undefined();
  }

  sp = sp - props_cnt * 2;
}

void NjsVM::exec_add_elements(int elements_cnt) {
  JSValue& val_array = rt_stack[sp - elements_cnt - 1];
  assert(val_array.tag == JSValue::ARRAY);
  JSArray *array = val_array.val.as_array;

  array->dense_array.resize(elements_cnt);

  for (u32 i = sp - elements_cnt, ele_idx = 0; i < sp; i++, ele_idx++) {
    array->dense_array[ele_idx].assign(rt_stack[i]);
    rt_stack[i].set_undefined();
  }

  sp = sp - elements_cnt;
}

void NjsVM::exec_push_str(int str_idx, bool atom) {
  rt_stack[sp].tag = atom ? JSValue::JS_ATOM : JSValue::STRING;

  if (atom) {
    rt_stack[sp].val.as_int64 = str_idx;
  }
  else {
    auto str = new PrimitiveString(str_pool.get_list()[str_idx]);
    rt_stack[sp].val.as_primitive_string = str;
  }

  sp += 1;
}

void NjsVM::exec_push_this() {
  auto *func_env = function_env();
  if (func_env != nullptr) {
    rt_stack[sp] = JSValue(func_env->This);
  }
  else {
    rt_stack[sp] = JSValue(&top_level_this);
  }
  sp += 1;
}

void NjsVM::exec_keypath_access(int key_cnt, bool get_ref) {

  JSValue& val_obj = rt_stack[sp - key_cnt - 1];

  // don't visit the last component of the keypath here
  for (u32 i = sp - key_cnt; i < sp - 1; i++) {
    assert(val_obj.is_object());
    if (Global::show_vm_exec_steps) {
      std::cout << "...visit key " << to_utf8_string(str_pool.get_list()[rt_stack[i].val.as_int64])
                << std::endl;
    }
    // val_obj is a reference, so we are directly modify the cell in the stack frame.
     val_obj = val_obj.val.as_object->get_prop(rt_stack[i].val.as_int64, false);
    rt_stack[i].set_undefined();
  }
  assert(val_obj.is_object());
  // visit the last component separately
  invoker_this = val_obj;
  if (Global::show_vm_exec_steps) {
    std::cout << "...visit key " << to_utf8_string(str_pool.get_list()[rt_stack[sp - 1].val.as_int64])
              << std::endl;
  }
  val_obj = val_obj.val.as_object->get_prop(rt_stack[sp - 1].val.as_int64, get_ref);
  rt_stack[sp - 1].set_undefined();

  sp = sp - key_cnt;
}

void NjsVM::exec_index_access(bool get_ref) {
  JSValue& index = rt_stack[sp - 1];
  JSValue& obj = rt_stack[sp - 2];

  assert(index.tag == JSValue::STRING
        || index.tag == JSValue::JS_ATOM
        || index.tag == JSValue::NUM_FLOAT);
  assert(obj.is_object());

  invoker_this = obj;

  if (obj.tag == JSValue::ARRAY) {

    if (index.tag == JSValue::NUM_FLOAT) {
      obj = obj.val.as_array->access_element(u32(index.val.as_float64), get_ref);
    }
    else if (index.tag == JSValue::STRING) {
      auto *prim_str = index.val.as_primitive_string;
      int64_t idx_num = prim_str->convert_to_index();
      if (idx_num != -1) {
        obj = obj.val.as_array->access_element(u32(idx_num), get_ref);
      }
      else {
        obj = obj.val.as_object->get_prop(prim_str, get_ref);
      }
    }
    else if (index.tag == JSValue::JS_ATOM) {
      assert(false);
    }
  }
  else {
    if (index.tag == JSValue::NUM_FLOAT) {
      u16string num_str = to_utf16_string(std::to_string(index.val.as_float64));
      auto *prim_str = new PrimitiveString(num_str);
      obj = obj.val.as_object->get_prop(prim_str, get_ref);
    }
    else if (index.tag == JSValue::STRING) {
      auto *prim_str = index.val.as_primitive_string;
      obj = obj.val.as_object->get_prop(prim_str, get_ref);
    }
    else if (index.tag == JSValue::JS_ATOM) {
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
      str_data = &str_pool.get_list()[val.val.as_int64];
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
  JSValue& lhs = rt_stack[sp - 2];
  JSValue& rhs = rt_stack[sp - 1];

  if (lhs.tag != rhs.tag) {
    bool equal = false;
    if (lhs.is_string_type() && rhs.is_string_type()) {
      equal = are_strings_equal(lhs, rhs);
    }
    lhs.set_undefined();
    lhs = JSValue(equal);
  }
  else {
    assert(lhs.tag != JSValue::HEAP_VAL && lhs.tag != JSValue::VALUE_HANDLE);
    auto tag = lhs.tag;

    if (tag == JSValue::NUM_FLOAT) {
      bool equal = lhs.val.as_float64 == rhs.val.as_float64;
      lhs = JSValue(equal);
    }
    else if (tag == JSValue::JS_ATOM || tag == JSValue::NUM_INT) {
      bool equal = lhs.val.as_int64 == rhs.val.as_int64;
      lhs = JSValue(equal);
    }
    else if (lhs.is_string_type()) {
      bool equal = are_strings_equal(lhs, rhs);
      lhs.set_undefined();
      lhs = JSValue(equal);
    }
    else if (lhs.tag_is(JSValue::UNDEFINED) || lhs.tag_is(JSValue::JS_NULL)) {
      lhs = JSValue(true);
    }
    else if (lhs.is_object()) {
      bool equal = lhs.val.as_object == rhs.val.as_object;
      lhs = JSValue(equal);
    }
  }
  if (flip) lhs.val.as_bool = !lhs.val.as_bool;
  rhs.set_undefined();
  sp -= 1;
}

double NjsVM::to_numeric_value(JSValue& val) {
  if (val.tag_is(JSValue::NUM_FLOAT)) return val.val.as_float64;
  if (val.tag_is(JSValue::NUM_INT)) return val.val.as_int64;
  assert(false);
}

void NjsVM::exec_comparison(InstType type) {
  JSValue& lhs = rt_stack[sp - 2];
  JSValue& rhs = rt_stack[sp - 1];

  bool res = false;
  if (lhs.tag_is(JSValue::NUM_FLOAT) && rhs.tag_is(JSValue::NUM_FLOAT)) {
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

  lhs = JSValue(res);
  rhs.set_undefined();
  sp -= 1;
}

}