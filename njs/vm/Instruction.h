#ifndef NJS_INSTRUCTIONS_H
#define NJS_INSTRUCTIONS_H

#include <cstdint>
#include <string>

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;

// has corresponding string representations
enum class OpType {
  neg = 0,

  add,
  sub,
  mul,
  div,
  mod,

  logi_and,
  logi_or,
  logi_not,

  bits_and,
  bits_or,
  bits_xor,
  bits_not,

  lsh,
  lshi,
  rsh,
  rshi,
  ursh,
  urshi,

  gt,
  lt,
  ge,
  le,
  ne,
  ne3,
  eq,
  eq3,

  inc,
  dec,

  push,
  push_check,
  push_i32,
  push_f64,
  push_str,
  push_bool,
  push_atom,
  push_func_this,
  push_global_this,
  push_null,
  push_undef,
  push_uninit,
  pop,
  pop_check,
  pop_drop,
  store,
  store_check,
  store_curr_func,
  prop_assign,
  var_deinit,
  var_deinit_range,
  var_undef,
  loop_var_renew,
  var_dispose,
  var_dispose_range,

  jmp,
  jmp_true,
  jmp_false,
  jmp_cond,

  jmp_pop,
  jmp_true_pop,
  jmp_false_pop,
  jmp_cond_pop,

  make_func,
  capture,
  make_obj,
  make_array,
  add_props,
  add_elements,
  get_prop_atom,
  get_prop_atom2,
  get_prop_index,
  get_prop_index2,
  set_prop_atom,
  set_prop_index,

  dyn_get_var,
  dyn_set_var,

  dup_stack_top,
  move_to_top1,
  move_to_top2,

  for_in_init,
  for_in_next,
  for_of_init,
  for_of_next,
  iter_end_jmp,

  js_in,
  js_instanceof,
  js_typeof,
  js_delete,

  call,
  js_new,
  ret,
  ret_undef,
  ret_err,
  // begin procedure, end procedure
  proc_call,
  proc_ret,

  regexp_build,

  halt,
  halt_err,
  nop,
};

struct Instruction {

  struct OperandType1 {
    int opr1;
    int opr2;
  };

  struct OperandType2 {
    u16 opr1;
    u16 opr2;
    u16 opr3;
    u16 opr4;
  };

  static Instruction num_imm(double num);
  static int get_stack_usage(OpType op_type);

  Instruction(OpType op, u16 opr1, u16 opr2, u16 opr3, u16 opr4);
  Instruction(OpType op, int opr1, int opr2);
  Instruction(OpType op, int opr1);
  explicit Instruction(OpType op);
  Instruction();

  std::string description() const;
  void swap_two_operands();
  bool is_jump_single_target() {
    return op_type == OpType::jmp
           || op_type == OpType::jmp_true
           || op_type == OpType::jmp_false
           || op_type == OpType::jmp_pop
           || op_type == OpType::jmp_true_pop
           || op_type == OpType::jmp_false_pop
           || op_type == OpType::iter_end_jmp;
  }

  bool is_jump_two_target() {
    return op_type == OpType::jmp_cond
           || op_type == OpType::jmp_cond_pop;
  }

  OpType op_type;
  union {
    double num_float;
    OperandType1 two;
    OperandType2 four;
  } operand;

};


} // namespace njs

#endif // NJS_INSTRUCTIONS_H