#ifndef NJS_INSTRUCTIONS_H
#define NJS_INSTRUCTIONS_H

#include <cstdint>
#include <string>

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;

// has corresponding string representations
enum class InstType {
  neg,

  add,
  sub,
  mul,
  div,

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

  push,
  push_check,
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

  fast_add,
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

  static int get_stack_usage(InstType op_type) {
    switch (op_type) {
      case InstType::neg:
        return 0;
      case InstType::add:
      case InstType::sub:
      case InstType::mul:
      case InstType::div:
      case InstType::logi_and:
      case InstType::logi_or:
      case InstType::logi_not:
      case InstType::bits_and:
      case InstType::bits_or:
      case InstType::bits_xor:
      case InstType::bits_not:
      case InstType::lsh:
      case InstType::lshi:
      case InstType::rsh:
      case InstType::rshi:
      case InstType::ursh:
      case InstType::urshi:
      case InstType::gt:
      case InstType::lt:
      case InstType::ge:
      case InstType::le:
      case InstType::ne:
      case InstType::ne3:
      case InstType::eq:
      case InstType::eq3:
        return -1;
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
      case InstType::xor_assign:
        return -1;
      case InstType::inc:
      case InstType::dec:
        return 0;
      case InstType::push:
      case InstType::push_check:
      case InstType::pushi:
      case InstType::push_str:
      case InstType::push_bool:
      case InstType::push_atom:
      case InstType::push_this:
      case InstType::push_null:
      case InstType::push_undef:
      case InstType::push_uninit:
        return 1;
      case InstType::pop:
      case InstType::pop_drop:
        return -1;
      case InstType::store:
        return 0;
      case InstType::prop_assign:
      case InstType::prop_compound_assign:
        return -2;
      case InstType::var_deinit_range:
      case InstType::var_undef:
      case InstType::var_dispose:
      case InstType::var_dispose_range:
        return 0;
      case InstType::dup_stack_top:
        return 1;
      case InstType::jmp:
      case InstType::jmp_true:
      case InstType::jmp_false:
      case InstType::jmp_cond:
        return 0;
      case InstType::pop_jmp:
      case InstType::pop_jmp_true:
      case InstType::pop_jmp_false:
      case InstType::pop_jmp_cond:
        return -1;
      case InstType::fast_add:
        return 1;
      case InstType::fast_assign:
        return 0;
      case InstType::make_func:
        return 1;
      case InstType::capture:
        return 0;
      case InstType::make_obj:
      case InstType::make_array:
        return 1;
      case InstType::add_props:     // need special handling
      case InstType::add_elements:  // need special handling
        return 0;
      case InstType::key_access:
        return 0;
      case InstType::index_access:
        return -1;
      case InstType::dyn_get_var:
        return 1;
      case InstType::call:    // need special handling
      case InstType::js_new:  // need special handling
      case InstType::ret:
      case InstType::ret_err:
      case InstType::halt:
      case InstType::halt_err:
      case InstType::nop:
        return 0;
    }
  }

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