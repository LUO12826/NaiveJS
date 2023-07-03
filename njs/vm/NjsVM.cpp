#include "NjsVM.h"

#include <iostream>

#include "njs/codegen/CodegenVisitor.h"
#include "njs/common/enums.h"

namespace njs {

NjsVM::NjsVM(CodegenVisitor& visitor): heap(600) {
  rt_stack = std::make_unique<JSValue[]>(max_stack_size);

  bytecode = std::move(visitor.bytecode);
  str_pool = std::move(visitor.str_pool);
  num_pool = std::move(visitor.num_pool);
  func_meta = std::move(visitor.func_meta);

  auto& global_sym_table = visitor.scope_chain[0].get_symbol_table();

  for (auto& sym_pair : global_sym_table) {
    global_object.props_index_map.emplace(std::u16string(sym_pair.first), sym_pair.second.index);
  }

  sp = global_sym_table.size() + frame_meta_size;
}

void NjsVM::run() {
  execute();

  std::cout << "end of execution VM state: " << std::endl;
  std::cout << rt_stack[sp - 1].description() << std::endl << std::endl;
}

void NjsVM::execute() {
  while (true) {
    Instruction& inst = bytecode[pc];
    pc++;
    switch (inst.op_type) {

      case InstType::add: break;
      case InstType::sub: break;
      case InstType::neg: break;
      case InstType::mul: break;
      case InstType::div: break;
      case InstType::logi_and: break;
      case InstType::logi_or: break;
      case InstType::logi_not: break;
      case InstType::push:
        exec_push(inst);
        break;
      case InstType::pushi:
        rt_stack[sp] = JSValue(inst.operand.num);
        sp += 1;
        break;
      case InstType::pop:
        exec_pop(inst);
        break;
      case InstType::jmp:
        pc = inst.operand.two.opr1;
        break;
      case InstType::je: break;
      case InstType::jne: break;
      case InstType::gt: break;
      case InstType::lt: break;
      case InstType::ge: break;
      case InstType::le: break;
      case InstType::ne: break;
      case InstType::eq: break;
      case InstType::call:
        exec_call(inst.operand.two.opr1);
        break;
      case InstType::ret:
        exec_return();
        break;
      case InstType::fast_add:
        exec_fast_add(inst);
        break;
      case InstType::make_func:
        exec_make_func(inst.operand.two.opr1);
        break;
      case InstType::halt:
        return;
      case InstType::nop:
        break;
    }
  }
}

u32 NjsVM::calc_var_address(ScopeType scope, int raw_index) {
  if (scope == ScopeType::GLOBAL) return 0 + frame_meta_size + raw_index;
  if (scope == ScopeType::FUNC) return frame_bottom_pointer + frame_meta_size + raw_index;

  assert(scope == ScopeType::FUNC_PARAM);
  assert(rt_stack[frame_bottom_pointer + 1].tag == JSValue::STACK_FRAME_META2);
  u32 arg_count = rt_stack[frame_bottom_pointer + 1].flag_bits;
  return frame_bottom_pointer - arg_count + raw_index;
}

void NjsVM::exec_fast_add(Instruction& inst) {

  auto lhs_scope = int_to_scope_type(inst.operand.four.opr1);
  auto rhs_scope = int_to_scope_type(inst.operand.four.opr3);

  JSValue& lhs_val = rt_stack[calc_var_address(lhs_scope, inst.operand.four.opr2)];
  JSValue& rhs_val = rt_stack[calc_var_address(rhs_scope, inst.operand.four.opr4)];

  if (lhs_val.tag == JSValue::NUMBER_FLOAT && rhs_val.tag == JSValue::NUMBER_FLOAT) {
    rt_stack[sp] = JSValue(lhs_val.val.as_float64 + rhs_val.val.as_float64);
    sp += 1;
  }
  else {
    // currently not support.
    assert(false);
  }
}

void NjsVM::exec_return() {
  u32 ret_val_addr = sp - 1;

  u32 old_frame_bottom = rt_stack[frame_bottom_pointer + 1].val.as_int;
  u32 arg_cnt = rt_stack[frame_bottom_pointer + 1].flag_bits;

  u32 old_sp = frame_bottom_pointer - arg_cnt - 1;
  u32 old_pc = rt_stack[frame_bottom_pointer].flag_bits;

  // restore old state
  pc = old_pc;
  sp = old_sp;
  frame_bottom_pointer = old_frame_bottom;

  // copy return value
  rt_stack[old_sp] = rt_stack[ret_val_addr];
  sp += 1;
}

void NjsVM::exec_push(Instruction& inst) {
  auto var_scope = int_to_scope_type(inst.operand.two.opr1);
  JSValue& val = rt_stack[calc_var_address(var_scope, inst.operand.two.opr2)];
  rt_stack[sp] = val;
  sp += 1;
}

void NjsVM::exec_pop(Instruction& inst) {
  auto var_scope = int_to_scope_type(inst.operand.two.opr1);
  JSValue& val = rt_stack[calc_var_address(var_scope, inst.operand.two.opr2)];
  val = rt_stack[sp - 1];
  sp -= 1;
}

void NjsVM::exec_make_func(int meta_idx) {
  auto& meta = func_meta[meta_idx];
  auto& name = str_pool[meta.name_index];

  auto *func = heap.new_object<JSFunction>(name, meta.code_address);
  rt_stack[sp] = JSValue(static_cast<JSObject *>(func));
  sp += 1;
}

void NjsVM::exec_call(int arg_count) {
  JSValue& func_val = rt_stack[sp - arg_count - 1];
  assert(func_val.tag == JSValue::OBJECT);
  assert(func_val.val.as_object->obj_class == ObjectClass::CLS_FUNCTION);

  // first cell of a function stack frame: return address and pointer to the function object.
  rt_stack[sp].tag = JSValue::STACK_FRAME_META1;
  rt_stack[sp].flag_bits = pc;
  rt_stack[sp].val.as_object = func_val.val.as_object;

  // second cell of a function stack frame: saved `frame_bottom_pointer` and arguments count
  rt_stack[sp + 1].tag = JSValue::STACK_FRAME_META2;
  rt_stack[sp + 1].flag_bits = arg_count;
  rt_stack[sp + 1].val.as_int = frame_bottom_pointer;

  frame_bottom_pointer = sp;
  sp = frame_bottom_pointer + 2;
  pc = static_cast<JSFunction *>(func_val.val.as_object)->code_address;
}

}