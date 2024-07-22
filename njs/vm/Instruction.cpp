#include "Instruction.h"

#include <cstdio>
#include "njs/common/enum_strings.h"

namespace njs {

Instruction Instruction::num_imm(double num) {
  Instruction inst(OpType::push_f64);
  inst.operand.num_float = num;
  return inst;
}

Instruction::Instruction(OpType op, int opr1, int opr2): op_type(op) {
  operand.two[0] = opr1;
  operand.two[1]  = opr2;
}

Instruction::Instruction(OpType op, int opr1): op_type(op) {
  operand.two[0] = opr1;
}

Instruction::Instruction(OpType op): op_type(op) {}

Instruction::Instruction(): op_type(OpType::nop) {}

void Instruction::swap_two_operands() {
  int temp = operand.two[0];
  operand.two[0] = operand.two[1];
  operand.two[1] = temp;
}

std::string Instruction::description() const {
#define OPR1 operand.two[0]
#define OPR2 operand.two[1]

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
    case OpType::init: sprintf(buffer, "init"); break;
    case OpType::neg: sprintf(buffer, "neg"); break;
    case OpType::add: sprintf(buffer, "add"); break;
    case OpType::sub: sprintf(buffer, "sub"); break;
    case OpType::mul: sprintf(buffer, "mul"); break;
    case OpType::div: sprintf(buffer, "div"); break;
    case OpType::mod: sprintf(buffer, "mod"); break;

    case OpType::inc:
      sprintf(buffer, "inc  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::dec:
      sprintf(buffer, "dec  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;

    case OpType::add_assign:
      sprintf(buffer, "add_assign  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::add_assign_keep:
      sprintf(buffer, "add_assign_keep  %s %d", scope_type_names_alt[OPR1], OPR2);
      break;
    case OpType::add_to_left: sprintf(buffer, "add_to_left"); break;

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

    case OpType::push_local_noderef: sprintf(buffer, "push_local_noderef  %d", OPR1); break;
    case OpType::push_local_noderef_check: sprintf(buffer, "push_local_noderef_check  %d", OPR1); break;
    case OpType::push_local: sprintf(buffer, "push_local  %d", OPR1); break;
    case OpType::push_local_check: sprintf(buffer, "push_local_check  %d", OPR1); break;
    case OpType::push_global: sprintf(buffer, "push_global  %d", OPR1); break;
    case OpType::push_global_check: sprintf(buffer, "push_global_check  %d", OPR1); break;
    case OpType::push_arg: sprintf(buffer, "push_arg  %d", OPR1); break;
    case OpType::push_arg_check: sprintf(buffer, "push_arg_check  %d", OPR1); break;
    case OpType::push_closure: sprintf(buffer, "push_closure  %d", OPR1); break;
    case OpType::push_closure_check: sprintf(buffer, "push_closure_check  %d", OPR1); break;

    case OpType::push_i32:
      sprintf(buffer, "push_i32  %d", OPR1);
      break;
    case OpType::push_f64:
      sprintf(buffer, "push_f64  %lf", operand.num_float);
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

    case OpType::pop_local: sprintf(buffer, "pop_local  %d", OPR1); break;
    case OpType::pop_local_check: sprintf(buffer, "pop_local_check  %d", OPR1); break;
    case OpType::pop_global: sprintf(buffer, "pop_global  %d", OPR1); break;
    case OpType::pop_global_check: sprintf(buffer, "pop_global_check  %d", OPR1); break;
    case OpType::pop_arg: sprintf(buffer, "pop_arg  %d", OPR1); break;
    case OpType::pop_arg_check: sprintf(buffer, "pop_arg_check  %d", OPR1); break;
    case OpType::pop_closure: sprintf(buffer, "pop_closure  %d", OPR1); break;
    case OpType::pop_closure_check: sprintf(buffer, "pop_closure_check  %d", OPR1); break;

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
    case OpType::dyn_get_var_undef:
      sprintf(buffer, "dyn_get_var_undef  %d", OPR1);
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
    case OpType::js_to_number:
      sprintf(buffer, "to_number");
      break;
    case OpType::call:
      sprintf(buffer, "call  argc:%d  has_this:%d", OPR1, OPR2);
      break;
    case OpType::js_new:
      sprintf(buffer, "js_new  argc:%d", OPR1);
      break;

    case OpType::ret: sprintf(buffer, "ret"); break;
    case OpType::ret_undef: sprintf(buffer, "ret_undef"); break;
    case OpType::ret_err: sprintf(buffer, "ret_err"); break;
    case OpType::await: sprintf(buffer, "await"); break;
    case OpType::yield: sprintf(buffer, "yield"); break;

    case OpType::proc_call:
      sprintf(buffer, "proc_call  at:%d", OPR1);
      break;
    case OpType::proc_ret:
      sprintf(buffer, "proc_ret");
      break;
    case OpType::regexp_build:
      sprintf(buffer, "regexp_build  pattern_atom: %d  flags:%d", OPR1, OPR2);
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
      sprintf(buffer, "(instruction description missed. fixme)");
  }

  return std::string(buffer);
}

static int op_stack_usage[] = {
    0, // init
    0, // neg

    -1, // add
    -1, // sub
    -1, // mul
    -1, // div
    -1, // mod

    -1, // logi_and
    -1, // logi_or
    0,  // logi_not

    -1, // bits_and
    -1, // bits_or
    -1, // bits_xor
    0,  // bits_not

    -1, // lsh
    0, // lshi
    -1, // rsh
    0, // rshi
    -1, // ursh
    0, // urshi

    -1, // gt
    -1, // lt
    -1, // ge
    -1, // le
    -1, // ne
    -1, // ne3
    -1, // eq
    -1, // eq3

    0,  // inc
    0,  // dec

    -1, // add_assign
    0,  // add_assign_keep
    -1, // add_to_left

    1,  // push_local_noderef
    1,  // push_local_noderef_check
    1,  // push_local
    1,  // push_local_check
    1,  // push_global
    1,  // push_global_check
    1,  // push_arg
    1,  // push_arg_check
    1,  // push_closure
    1,  // push_closure_check

    1,  // push_i32
    1,  // push_f64
    1,  // push_str
    1,  // push_bool
    1,  // push_atom
    1,  // push_func_this
    1,  // push_global_this
    1,  // push_null
    1,  // push_undef
    1,  // push_uninit

    -1, // pop_local
    -1, // pop_local_check
    -1, // pop_global
    -1, // pop_global_check
    -1, // pop_arg
    -1, // pop_arg_check
    -1, // pop_closure
    -1, // pop_closure_check

    -1, // pop
    -1, // pop_check
    -1, // pop_drop
    0,  // store
    0,  // store_check
    0,  // store_curr_func
    0,  // var_deinit
    0,  // var_deinit_range
    0,  // var_undef
    0,  // loop_var_renew
    0,  // var_dispose
    0,  // var_dispose_range

    0,  // jmp
    0,  // jmp_true
    0,  // jmp_false
    0,  // jmp_cond

    -1, // jmp_pop
    -1, // jmp_true_pop
    -1, // jmp_false_pop
    -1, // jmp_cond_pop

    1,  // make_func
    1,  // make_obj
    1,  // make_array
    0,  // add_props
    0,  // add_elements
    0,  // get_prop_atom
    1,  // get_prop_atom2
    -1, // get_prop_index
    0,  // get_prop_index2
    -1, // set_prop_atom
    -2, // set_prop_index

    1,  // dyn_get_var
    1,  // dyn_get_var_undef
    0,  // dyn_set_var

    1,  // dup_stack_top
    0,  // move_to_top1
    0,  // move_to_top2

    0,  // for_in_init
    2,  // for_in_next
    0,  // for_of_init
    2,  // for_of_next
    0,  // iter_end_jmp

    -1, // js_in
    -1, // js_instanceof
    0,  // js_typeof
    -1, // js_delete
    0,  // js_to_number

    0,  // call
    0,  // js_new
    0,  // ret
    0,  // ret_undef
    0,  // ret_err
    0,  // await
    0,  // yield
    1,  // proc_call
    -1, // proc_ret

    1,  // regexp_build

    0,  // halt
    0,  // halt_err
    0   // nop
};


int Instruction::get_stack_usage(OpType op_type) {
  return op_stack_usage[static_cast<size_t>(op_type)];
}

}