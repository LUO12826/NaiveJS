#include "Instructions.h"

#include "njs/common/enum_strings.h"
#include <cstdio>

namespace njs {

Instruction Instruction::num_imm(double num) {
  Instruction inst(InstType::pushi);
  inst.operand.num_float = num;
  return inst;
}

Instruction::Instruction(InstType op, u16 opr1, u16 opr2, u16 opr3, u16 opr4): op_type(op) {
  operand.four.opr1 = opr1;
  operand.four.opr2 = opr2;
  operand.four.opr3 = opr3;
  operand.four.opr4 = opr4;
}

Instruction::Instruction(InstType op, int opr1, int opr2): op_type(op) {
  operand.two.opr1 = opr1;
  operand.two.opr2 = opr2;
}

Instruction::Instruction(InstType op, int opr1): op_type(op) {
  operand.two.opr1 = opr1;
}

Instruction::Instruction(InstType op): op_type(op) {}

Instruction::Instruction(): op_type(InstType::nop) {}

void Instruction::swap_two_operands() {
  int temp = operand.two.opr1;
  operand.two.opr1 = operand.two.opr2;
  operand.two.opr2 = temp;
}

std::string Instruction::description() {

  char buffer[80];

  switch (op_type) {
    case InstType::add: sprintf(buffer, "add"); break;
    case InstType::sub: sprintf(buffer, "sub"); break;
    case InstType::mul: sprintf(buffer, "mul"); break;
    case InstType::div: sprintf(buffer, "div"); break;
    case InstType::neg: sprintf(buffer, "neg"); break;

    case InstType::add_assign:
      sprintf(buffer, "add_assign  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::sub_assign:
      sprintf(buffer, "sub_assign  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::mul_assign:
      sprintf(buffer, "mul_assign  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::div_assign:
      sprintf(buffer, "div_assign  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;

    case InstType::inc:
      sprintf(buffer, "inc  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;

    case InstType::dec:
      sprintf(buffer, "dec  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;

    case InstType::le: sprintf(buffer, "le"); break;
    case InstType::ge: sprintf(buffer, "ge"); break;
    case InstType::lt: sprintf(buffer, "lt"); break;
    case InstType::gt: sprintf(buffer, "gt"); break;

    case InstType::ne: sprintf(buffer, "ne"); break;
    case InstType::ne3: sprintf(buffer, "ne3"); break;
    case InstType::eq: sprintf(buffer, "eq"); break;
    case InstType::eq3: sprintf(buffer, "eq3"); break;

    case InstType::logi_and: sprintf(buffer, "logi_and"); break;
    case InstType::logi_or: sprintf(buffer, "logi_or"); break;
    case InstType::logi_not: sprintf(buffer, "logi_not"); break;

    case InstType::fast_add:
      sprintf(buffer, "fast_add  %s %hu %s %hu",
                      scope_type_names[operand.four.opr1], operand.four.opr2,
                      scope_type_names[operand.four.opr3], operand.four.opr4);
      break;
    case InstType::fast_assign:
      sprintf(buffer, "fast_assign  %s %hu %s %hu",
                      scope_type_names[operand.four.opr1], operand.four.opr2,
                      scope_type_names[operand.four.opr3], operand.four.opr4);
      break;
    case InstType::push:
      sprintf(buffer, "push  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::pushi:
      sprintf(buffer, "pushi  %lf", operand.num_float);
      break;
    case InstType::push_str:
      sprintf(buffer, "push_str  %d", operand.two.opr1);
      break;
    case InstType::push_bool:
      sprintf(buffer, "push_bool  %d", operand.two.opr1);
      break;
    case InstType::push_atom:
      sprintf(buffer, "push_atom  %d", operand.two.opr1);
      break;
    case InstType::push_this:
      sprintf(buffer, "push_this  %d", operand.two.opr1);
      break;
    case InstType::push_null:
      sprintf(buffer, "push_null");
      break;
    case InstType::push_undef:
      sprintf(buffer, "push_undef");
      break;
    case InstType::pop:
      sprintf(buffer, "pop  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::pop_drop:
      sprintf(buffer, "pop_drop");
      break;
    case InstType::store:
      sprintf(buffer, "store  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::prop_assign:
      sprintf(buffer, "prop_assign");
      break;
    case InstType::var_dispose:
      sprintf(buffer, "var_dispose  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::dup_stack_top:
      sprintf(buffer, "dup_stack_top");
      break;
    case InstType::jmp:
      sprintf(buffer, "jmp  %d", operand.two.opr1);
      break;
    case InstType::jmp_true:
      sprintf(buffer, "jmp_true  %d", operand.two.opr1);
      break;
    case InstType::jmp_false:
      sprintf(buffer, "jmp_false  %d", operand.two.opr1);
      break;
    case InstType::pop_jmp:
      sprintf(buffer, "pop_jmp  %d", operand.two.opr1);
      break;
    case InstType::pop_jmp_true:
      sprintf(buffer, "pop_jmp_true  %d", operand.two.opr1);
      break;
    case InstType::pop_jmp_false:
      sprintf(buffer, "pop_jmp_false  %d", operand.two.opr1);
      break;
    case InstType::jmp_cond:
      sprintf(buffer, "jmp_cond  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case InstType::make_func:
      sprintf(buffer, "make_func  %d", operand.two.opr1);
      break;
    case InstType::capture:
      sprintf(buffer, "capture %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::make_obj:
      sprintf(buffer, "make_obj");
      break;
    case InstType::make_array:
      sprintf(buffer, "make_array");
      break;
    case InstType::add_props:
      sprintf(buffer, "add_props  %d", operand.two.opr1);
      break;
    case InstType::add_elements:
      sprintf(buffer, "add_elements  %d", operand.two.opr1);
      break;
    case InstType::keypath_access:
      sprintf(buffer, "keypath_access  %d %s", operand.two.opr1, operand.two.opr2 ? "REF" : "");
      break;
    case InstType::index_access:
      sprintf(buffer, "index_access  %s", operand.two.opr1 ? "REF" : "");
      break;
    case InstType::call:
      sprintf(buffer, "call  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case InstType::ret:
      sprintf(buffer, "ret");
      break;
    case InstType::ret_err:
      sprintf(buffer, "ret_err");
      break;
    case InstType::halt:
      sprintf(buffer, "halt");
      break;
    case InstType::halt_err:
      sprintf(buffer, "halt_err");
      break;
    case InstType::nop:
      sprintf(buffer, "nop");
      break;
    default:
      // fixme
      sprintf(buffer, "(instruction description missed. fixme)");
  }

  return std::string(buffer);
}

}