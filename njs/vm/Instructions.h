#ifndef NJS_INSTRUCTIONS_H
#define NJS_INSTRUCTIONS_H

#include <cstdint>
#include <string>

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;

// has corresponding string representations
enum class InstType {
  add,
  sub,
  neg,
  mul,
  div,

  add_assign,
  sub_assign,
  mul_assign,
  div_assign,
  mod_assign,
  lsh_assign,
  rsh_assign,
  ursh_assign,
  and_assign,
  or_assign,
  xor_assign,

  inc,
  dec,

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

  push,
  pushi,
  push_str,
  push_bool,
  push_atom,
  push_this,
  push_null,
  push_undef,
  push_uninit,
  pop,
  pop_drop,
  store,
  prop_assign,
  prop_compound_assign,
  var_deinit_range,
  var_undef,
  var_dispose,
  var_dispose_range,
  dup_stack_top,

  jmp,
  jmp_true,
  jmp_false,
  jmp_cond,

  pop_jmp,
  pop_jmp_true,
  pop_jmp_false,
  pop_jmp_cond,

  gt,
  lt,
  ge,
  le,
  ne,
  ne3,
  eq,
  eq3,

  fast_add,
  fast_bin,
  fast_assign,

  make_func,
  capture,
  make_obj,
  make_array,
  add_props,
  add_elements,
  key_access,
  index_access,

  dyn_get_var,

  call,
  js_new,
  ret,
  ret_err,

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

  Instruction(InstType op, u16 opr1, u16 opr2, u16 opr3, u16 opr4);

  Instruction(InstType op, int opr1, int opr2);

  Instruction(InstType op, int opr1);  

  explicit Instruction(InstType op);

  Instruction();

  std::string description() const;

  void swap_two_operands();

  InstType op_type;
  union {
    double num_float;
    OperandType1 two;
    OperandType2 four;
  } operand;

};


} // namespace njs

#endif // NJS_INSTRUCTIONS_H