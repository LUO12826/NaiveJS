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
#define OPR1 operand.two.opr1
#define OPR2 operand.two.opr2

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
      sprintf(buffer, "inc  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;

    case OpType::dec:
      sprintf(buffer, "dec  %s %d", scope_type_names_alt[OPR1], OPR2);
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
    case OpType::lshi: sprintf(buffer, "lshi %u", (u32)OPR1); break;
    case OpType::rsh: sprintf(buffer, "rsh"); break;
    case OpType::rshi: sprintf(buffer, "rshi %u", (u32)OPR1); break;
    case OpType::ursh: sprintf(buffer, "ursh"); break;
    case OpType::urshi: sprintf(buffer, "urshi %u", (u32)OPR1); break;

    case OpType::push:
      sprintf(buffer, "push  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::push_check:
      sprintf(buffer, "push_check  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::pushi:
      sprintf(buffer, "pushi  %lf", operand.num_float);
      break;
    case OpType::push_str:
      sprintf(buffer, "push_str  %u", OPR1);
      break;
    case OpType::push_bool:
      sprintf(buffer, "push_bool  %d", OPR1);
      break;
    case OpType::push_atom:
      sprintf(buffer, "push_atom  %u", (u32)OPR1);
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
      sprintf(buffer, "pop  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::pop_check:
      sprintf(buffer, "pop_check  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::pop_drop:
      sprintf(buffer, "pop_drop");
      break;
    case OpType::store:
      sprintf(buffer, "store  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::store_check:
      sprintf(buffer, "store_check  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::store_curr_func:
      sprintf(buffer, "store_curr_func  %d", OPR1);
      break;
    case OpType::prop_assign:
      sprintf(buffer, "prop_assign %s", (bool)OPR1 ? "(need value)" : "");
      break;
    case OpType::var_deinit:
      sprintf(buffer, "var_deinit  %d", OPR1);
      break;
    case OpType::var_deinit_range:
      sprintf(buffer, "var_deinit_range  %d %d", OPR1, OPR2);
      break;
    case OpType::var_undef:
      sprintf(buffer, "var_undef %d", OPR1);
      break;
    case OpType::loop_var_renew:
      sprintf(buffer, "loop_var_renew %d", OPR1);
      break;
    case OpType::var_dispose:
      sprintf(buffer, "var_dispose  %d", OPR1);
      break;
    case OpType::var_dispose_range:
      sprintf(buffer, "var_dispose_range  %d %d", OPR1, OPR2);
      break;
    case OpType::jmp:
      sprintf(buffer, "jmp  %d", OPR1);
      break;
    case OpType::jmp_true:
      sprintf(buffer, "jmp_true  %d", OPR1);
      break;
    case OpType::jmp_false:
      sprintf(buffer, "jmp_false  %d", OPR1);
      break;
    case OpType::jmp_cond:
      sprintf(buffer, "jmp_cond  %d %d", OPR1, OPR2);
      break;
    case OpType::jmp_pop:
      sprintf(buffer, "jmp_pop  %d", OPR1);
      break;
    case OpType::jmp_true_pop:
      sprintf(buffer, "jmp_true_pop  %d", OPR1);
      break;
    case OpType::jmp_false_pop:
      sprintf(buffer, "jmp_false_pop  %d", OPR1);
      break;
    case OpType::jmp_cond_pop:
      sprintf(buffer, "jmp_cond_pop  %d %d", OPR1, OPR2);
      break;
    case OpType::make_func:
      sprintf(buffer, "make_func  %d", OPR1);
      break;
    case OpType::capture:
      sprintf(buffer, "capture %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::make_obj:
      sprintf(buffer, "make_obj");
      break;
    case OpType::make_array:
      sprintf(buffer, "make_array");
      break;
    case OpType::add_props:
      sprintf(buffer, "add_props  cnt:%d", OPR1);
      break;
    case OpType::add_elements:
      sprintf(buffer, "add_elements  cnt:%d", OPR1);
      break;
    case OpType::get_prop_atom:
      sprintf(buffer, "get_prop_atom  %d", OPR1);
      break;
    case OpType::get_prop_atom2:
      sprintf(buffer, "get_prop_atom2  %d", OPR1);
      break;
    case OpType::get_prop_index:
      sprintf(buffer, "get_prop_index");
      break;
    case OpType::get_prop_index2:
      sprintf(buffer, "get_prop_index2");
      break;
    case OpType::set_prop_atom:
      sprintf(buffer, "set_prop_atom  %d", OPR1);
      break;
    case OpType::set_prop_index:
      sprintf(buffer, "set_prop_index");
      break;
    case OpType::dyn_get_var:
      sprintf(buffer, "dyn_get_var  %d", OPR1);
      break;
    case OpType::dyn_set_var:
      sprintf(buffer, "dyn_set_var  %d", OPR1);
      break;
    case OpType::dup_stack_top:
      sprintf(buffer, "dup_stack_top");
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
    case OpType::for_of_init:
      sprintf(buffer, "for_of_init");
      break;
    case OpType::for_of_next:
      sprintf(buffer, "for_of_next");
      break;
    case OpType::iter_end_jmp:
      sprintf(buffer, "iter_end_jmp  to:%d", OPR1);
      break;
    case OpType::js_in:
      sprintf(buffer, "in");
      break;
    case OpType::js_instanceof:
      sprintf(buffer, "instanceof");
      break;
    case OpType::js_typeof:
      sprintf(buffer, "typeof");
      break;
    case OpType::js_delete:
      sprintf(buffer, "delete");
      break;
    case OpType::call:
      sprintf(buffer, "call  argc:%d  has_this:%d", OPR1, OPR2);
      break;
    case OpType::js_new:
      sprintf(buffer, "js_new  argc:%d", OPR1);
      break;
    case OpType::ret:
      sprintf(buffer, "ret");
      break;
    case OpType::ret_err:
      sprintf(buffer, "ret_err");
      break;
    case OpType::proc_call:
      sprintf(buffer, "proc_call  at:%d", OPR1);
      break;
    case OpType::proc_ret:
      sprintf(buffer, "proc_ret");
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

int Instruction::get_stack_usage(OpType op_type) {
  switch (op_type) {
    case OpType::neg:
      return 0;
    case OpType::add:
    case OpType::sub:
    case OpType::mul:
    case OpType::div:
    case OpType::mod:
    case OpType::logi_and:
    case OpType::logi_or:
      return -1;
    case OpType::logi_not:
      return 0;
    case OpType::bits_and:
    case OpType::bits_or:
    case OpType::bits_xor:
      return -1;
    case OpType::bits_not:
      return 0;
    case OpType::lsh:
    case OpType::lshi:
    case OpType::rsh:
    case OpType::rshi:
    case OpType::ursh:
    case OpType::urshi:
    case OpType::gt:
    case OpType::lt:
    case OpType::ge:
    case OpType::le:
    case OpType::ne:
    case OpType::ne3:
    case OpType::eq:
    case OpType::eq3:
      return -1;
    case OpType::inc:
    case OpType::dec:
      return 0;
    case OpType::push:
    case OpType::push_check:
    case OpType::pushi:
    case OpType::push_str:
    case OpType::push_bool:
    case OpType::push_atom:
    case OpType::push_func_this:
    case OpType::push_global_this:
    case OpType::push_null:
    case OpType::push_undef:
    case OpType::push_uninit:
      return 1;
    case OpType::pop:
    case OpType::pop_check:
    case OpType::pop_drop:
      return -1;
    case OpType::store:
    case OpType::store_check:
      return 0;
    case OpType::prop_assign:
      return -2;
    case OpType::var_deinit:
    case OpType::var_deinit_range:
    case OpType::var_undef:
    case OpType::loop_var_renew:
    case OpType::var_dispose:
    case OpType::var_dispose_range:
      return 0;
    case OpType::jmp:
    case OpType::jmp_true:
    case OpType::jmp_false:
    case OpType::jmp_cond:
      return 0;
    case OpType::jmp_pop:
    case OpType::jmp_true_pop:
    case OpType::jmp_false_pop:
    case OpType::jmp_cond_pop:
      return -1;
    case OpType::make_func:
      return 1;
    case OpType::capture:
      return 0;
    case OpType::make_obj:
    case OpType::make_array:
      return 1;
    case OpType::add_props:     // need special handling
    case OpType::add_elements:  // need special handling
      return 0;
    case OpType::get_prop_atom:
      return 0;
    case OpType::get_prop_atom2:
      return 1;
    case OpType::get_prop_index:
      return -1;
    case OpType::get_prop_index2:
      return 0;
    case OpType::set_prop_atom:
      return -1;
    case OpType::set_prop_index:
      return -2;
    case OpType::dyn_get_var:
      return 1;
    case OpType::dup_stack_top:
      return 1;
    case OpType::for_in_init:
    case OpType::for_of_init:
      return 0;
    case OpType::for_in_next:
    case OpType::for_of_next:
      return 2;
    case OpType::js_in:
    case OpType::js_instanceof:
      return -1;
    case OpType::js_typeof:
      return 0;
    case OpType::js_delete:
      return -1;
    case OpType::call:    // need special handling
    case OpType::js_new:  // need special handling
    case OpType::iter_end_jmp: // need special handling
      return 0;
    case OpType::proc_call:
      return 1;
    case OpType::proc_ret:
      return -1;
    default:
      return 0;
  }
}

}