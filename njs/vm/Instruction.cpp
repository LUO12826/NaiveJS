#include "Instruction.h"

#include "njs/common/enum_strings.h"
#include <cstdio>

namespace njs {

Instruction Instruction::num_imm(double num) {
  Instruction inst(OpType::pushi);
  inst.operand.num_float = num;
  return inst;
}

Instruction::Instruction(OpType op, u16 opr1, u16 opr2, u16 opr3, u16 opr4): op_type(op) {
  operand.four.opr1 = opr1;
  operand.four.opr2 = opr2;
  operand.four.opr3 = opr3;
  operand.four.opr4 = opr4;
}

Instruction::Instruction(OpType op, int opr1, int opr2): op_type(op) {
  operand.two.opr1 = opr1;
  operand.two.opr2 = opr2;
}

Instruction::Instruction(OpType op, int opr1): op_type(op) {
  operand.two.opr1 = opr1;
}

Instruction::Instruction(OpType op): op_type(op) {}

Instruction::Instruction(): op_type(OpType::nop) {}

void Instruction::swap_two_operands() {
  int temp = operand.two.opr1;
  operand.two.opr1 = operand.two.opr2;
  operand.two.opr2 = temp;
}

std::string Instruction::description() const {

  static const char *assign_op_names[] = {
      "add_assign",
      "sub_assign",
      "mul_assign",
      "div_assign",
      "mod_assign",
      "lsh_assign",
      "rsh_assign",
      "ursh_assign",
      "and_assign",
      "or_assign",
      "xor_assign",
  };

  char buffer[80];

  switch (op_type) {
    case OpType::add: sprintf(buffer, "add"); break;
    case OpType::sub: sprintf(buffer, "sub"); break;
    case OpType::mul: sprintf(buffer, "mul"); break;
    case OpType::div: sprintf(buffer, "div"); break;
    case OpType::mod: sprintf(buffer, "mod"); break;
    case OpType::neg: sprintf(buffer, "neg"); break;

    case OpType::inc:
      sprintf(buffer, "inc  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;

    case OpType::dec:
      sprintf(buffer, "dec  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;

    case OpType::le: sprintf(buffer, "le"); break;
    case OpType::ge: sprintf(buffer, "ge"); break;
    case OpType::lt: sprintf(buffer, "lt"); break;
    case OpType::gt: sprintf(buffer, "gt"); break;

    case OpType::ne: sprintf(buffer, "ne"); break;
    case OpType::ne3: sprintf(buffer, "ne3"); break;
    case OpType::eq: sprintf(buffer, "eq"); break;
    case OpType::eq3: sprintf(buffer, "eq3"); break;

    case OpType::logi_and: sprintf(buffer, "logi_and"); break;
    case OpType::logi_or: sprintf(buffer, "logi_or"); break;
    case OpType::logi_not: sprintf(buffer, "logi_not"); break;

    case OpType::bits_and: sprintf(buffer, "bits_and"); break;
    case OpType::bits_or: sprintf(buffer, "bits_or"); break;
    case OpType::bits_xor: sprintf(buffer, "bits_xor"); break;
    case OpType::bits_not: sprintf(buffer, "bits_not"); break;

    case OpType::lsh: sprintf(buffer, "lsh"); break;
    case OpType::lshi: sprintf(buffer, "lshi %u", (u32)operand.two.opr1); break;
    case OpType::rsh: sprintf(buffer, "rsh"); break;
    case OpType::rshi: sprintf(buffer, "rshi %u", (u32)operand.two.opr1); break;
    case OpType::ursh: sprintf(buffer, "ursh"); break;
    case OpType::urshi: sprintf(buffer, "urshi %u", (u32)operand.two.opr1); break;

    case OpType::push:
      sprintf(buffer, "push  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::push_check:
      sprintf(buffer, "push_check  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::pushi:
      sprintf(buffer, "pushi  %lf", operand.num_float);
      break;
    case OpType::push_str:
      sprintf(buffer, "push_str  %d", operand.two.opr1);
      break;
    case OpType::push_bool:
      sprintf(buffer, "push_bool  %d", operand.two.opr1);
      break;
    case OpType::push_atom:
      sprintf(buffer, "push_atom  %u", (u32)operand.two.opr1);
      break;
    case OpType::push_func_this:
      sprintf(buffer, "push_this");
      break;
    case OpType::push_global_this:
      sprintf(buffer, "push_global_this");
      break;
    case OpType::push_null:
      sprintf(buffer, "push_null");
      break;
    case OpType::push_undef:
      sprintf(buffer, "push_undef");
      break;
    case OpType::push_uninit:
      sprintf(buffer, "push_uninit");
      break;
    case OpType::pop:
      sprintf(buffer, "pop  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::pop_check:
      sprintf(buffer, "pop_check  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::pop_drop:
      sprintf(buffer, "pop_drop");
      break;
    case OpType::store:
      sprintf(buffer, "store  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::store_check:
      sprintf(buffer, "store_check  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::prop_assign:
      sprintf(buffer, "prop_assign %s", (bool)operand.two.opr1 ? "(need value)" : "");
      break;
    case OpType::var_deinit_range:
      sprintf(buffer, "var_deinit_range  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case OpType::var_undef:
      sprintf(buffer, "var_undef %d", operand.two.opr1);
      break;
    case OpType::loop_var_renew:
      sprintf(buffer, "loop_var_renew %d", operand.two.opr1);
      break;
    case OpType::var_dispose:
      sprintf(buffer, "var_dispose  %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::var_dispose_range:
      sprintf(buffer, "var_dispose_range  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case OpType::jmp:
      sprintf(buffer, "jmp  %d", operand.two.opr1);
      break;
    case OpType::jmp_true:
      sprintf(buffer, "jmp_true  %d", operand.two.opr1);
      break;
    case OpType::jmp_false:
      sprintf(buffer, "jmp_false  %d", operand.two.opr1);
      break;
    case OpType::jmp_cond:
      sprintf(buffer, "jmp_cond  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case OpType::pop_jmp:
      sprintf(buffer, "pop_jmp  %d", operand.two.opr1);
      break;
    case OpType::pop_jmp_true:
      sprintf(buffer, "pop_jmp_true  %d", operand.two.opr1);
      break;
    case OpType::pop_jmp_false:
      sprintf(buffer, "pop_jmp_false  %d", operand.two.opr1);
      break;
    case OpType::pop_jmp_cond:
      sprintf(buffer, "pop_jmp_cond  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case OpType::make_func:
      sprintf(buffer, "make_func  %d", operand.two.opr1);
      break;
    case OpType::capture:
      sprintf(buffer, "capture %s %d", scope_type_names_alt[operand.two.opr1], operand.two.opr2);
      break;
    case OpType::make_obj:
      sprintf(buffer, "make_obj");
      break;
    case OpType::make_array:
      sprintf(buffer, "make_array");
      break;
    case OpType::add_props:
      sprintf(buffer, "add_props  %d", operand.two.opr1);
      break;
    case OpType::add_elements:
      sprintf(buffer, "add_elements  %d", operand.two.opr1);
      break;
    case OpType::get_prop_atom:
      sprintf(buffer, "get_prop_atom  %d", operand.two.opr1);
      break;
    case OpType::get_prop_atom2:
      sprintf(buffer, "get_prop_atom2  %d", operand.two.opr1);
      break;
    case OpType::get_prop_index:
      sprintf(buffer, "get_prop_index");
      break;
    case OpType::get_prop_index2:
      sprintf(buffer, "get_prop_index2");
      break;
    case OpType::set_prop_atom:
      sprintf(buffer, "set_prop_atom  %d", operand.two.opr1);
      break;
    case OpType::set_prop_index:
      sprintf(buffer, "set_prop_index");
      break;
    case OpType::dyn_get_var:
      sprintf(buffer, "dyn_get_var  %d", operand.two.opr1);
      break;
    case OpType::dyn_set_var:
      sprintf(buffer, "dyn_set_var  %d", operand.two.opr1);
      break;
    case OpType::move_to_top1:
      sprintf(buffer, "move_to_top1");
      break;
    case OpType::move_to_top2:
      sprintf(buffer, "move_to_top2");
      break;
    case OpType::for_in_init:
      sprintf(buffer, "for_in_init");
      break;
    case OpType::for_in_next:
      sprintf(buffer, "for_in_next");
      break;
    case OpType::iter_end_jmp:
      sprintf(buffer, "iter_end_jmp  %d", operand.two.opr1);
      break;
    case OpType::call:
      sprintf(buffer, "call  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case OpType::js_new:
      sprintf(buffer, "js_new  %d", operand.two.opr1);
      break;
    case OpType::ret:
      sprintf(buffer, "ret");
      break;
    case OpType::ret_err:
      sprintf(buffer, "ret_err");
      break;
    case OpType::halt:
      sprintf(buffer, "halt");
      break;
    case OpType::halt_err:
      sprintf(buffer, "halt_err");
      break;
    case OpType::nop:
      sprintf(buffer, "nop");
      break;
    default:
      // fixme
      sprintf(buffer, "(instruction description missed. fixme)");
  }

  return std::string(buffer);
}

}