#include "Instructions.h"

#include <cstdio>
#include "njs/parser/enum_strings.h"

namespace njs {

Instruction Instruction::num_imm(double num) {
  Instruction inst(InstType::pushi);
  inst.operand.num = num;
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

std::string Instruction::description() {

  char buffer[100];

  switch (op_type) {
    case InstType::add:
      sprintf(buffer, "add");
      break;
    case InstType::fast_add:
      sprintf(buffer, "fast_add  %s %hu %s %hu",
                      scope_type_names[operand.four.opr1],
              operand.four.opr2,
                      scope_type_names[operand.four.opr3], operand.four.opr4);
      break;
    case InstType::push:
      sprintf(buffer, "push  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::pushi:
      sprintf(buffer, "pushi  %lf", operand.num);
      break;
    case InstType::pop:
      sprintf(buffer, "pop  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::jmp:
      sprintf(buffer, "jmp  %d", operand.two.opr1);
      break;
    case InstType::make_func:
      sprintf(buffer, "make_func  %d", operand.two.opr1);
      break;
    case InstType::call:
      sprintf(buffer, "call  %d", operand.two.opr1);
      break;
    case InstType::ret:
      sprintf(buffer, "ret");
      break;
    case InstType::halt:
      sprintf(buffer, "halt");
      break;
    default:
      // fixme
      sprintf(buffer, "(fixme)");
  }

  return std::string(buffer);
}

}