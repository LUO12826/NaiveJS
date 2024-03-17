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
    case InstType::add: sprintf(buffer, "add"); break;
    case InstType::sub: sprintf(buffer, "sub"); break;
    case InstType::mul: sprintf(buffer, "mul"); break;
    case InstType::div: sprintf(buffer, "div"); break;
    case InstType::neg: sprintf(buffer, "neg"); break;

    case InstType::add_assign:
    case InstType::sub_assign:
    case InstType::mul_assign:
    case InstType::div_assign:
    case InstType::mod_assign:
    case InstType::lsh_assign:
    case InstType::rsh_assign:
    case InstType::ursh_assign:
    case InstType::and_assign:
    case InstType::or_assign:
    case InstType::xor_assign: {
      int index = static_cast<int>(op_type) - static_cast<int>(InstType::add_assign);
      sprintf(buffer, "%s  %s %d", assign_op_names[index], scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    }
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

    case InstType::bits_and: sprintf(buffer, "bits_and"); break;
    case InstType::bits_or: sprintf(buffer, "bits_or"); break;
    case InstType::bits_xor: sprintf(buffer, "bits_xor"); break;
    case InstType::bits_not: sprintf(buffer, "bits_not"); break;

    case InstType::lsh: sprintf(buffer, "lsh"); break;
    case InstType::lshi: sprintf(buffer, "lshi %u", (u32)operand.two.opr1); break;
    case InstType::rsh: sprintf(buffer, "rsh"); break;
    case InstType::rshi: sprintf(buffer, "rshi %u", (u32)operand.two.opr1); break;
    case InstType::ursh: sprintf(buffer, "ursh"); break;
    case InstType::urshi: sprintf(buffer, "urshi %u", (u32)operand.two.opr1); break;

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
    case InstType::push_uninit:
      sprintf(buffer, "push_uninit");
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
      sprintf(buffer, "prop_assign %s", (bool)operand.two.opr1 ? "(need value)" : "");
      break;
    case InstType::prop_compound_assign:
      sprintf(buffer, "prop_compound_assign %s  %s",
              assign_op_names[operand.two.opr1 - static_cast<int>(InstType::add_assign)],
              (bool)operand.two.opr2 ? "(need value)" : "");
      break;
    case InstType::var_deinit_range:
      sprintf(buffer, "var_deinit_range  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case InstType::var_undef:
      sprintf(buffer, "var_undef %d", operand.two.opr1);
      break;
    case InstType::var_dispose:
      sprintf(buffer, "var_dispose  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::var_dispose_range:
      sprintf(buffer, "var_dispose_range  %d %d", operand.two.opr1, operand.two.opr2);
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
    case InstType::jmp_cond:
      sprintf(buffer, "jmp_cond  %d %d", operand.two.opr1, operand.two.opr2);
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
    case InstType::pop_jmp_cond:
      sprintf(buffer, "pop_jmp_cond  %d %d", operand.two.opr1, operand.two.opr2);
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
    case InstType::key_access:
      sprintf(buffer, "key_access  %d %s", operand.two.opr1, operand.two.opr2 ? "REF" : "");
      break;
    case InstType::index_access:
      sprintf(buffer, "index_access  %s", operand.two.opr1 ? "REF" : "");
      break;
    case InstType::dyn_get_var:
      sprintf(buffer, "dyn_get_var  %s %d", scope_type_names[operand.two.opr1], operand.two.opr2);
      break;
    case InstType::call:
      sprintf(buffer, "call  %d %d", operand.two.opr1, operand.two.opr2);
      break;
    case InstType::js_new:
      sprintf(buffer, "js_new  %d", operand.two.opr1);
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