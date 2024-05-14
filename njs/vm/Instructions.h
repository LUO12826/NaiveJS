#ifndef NJS_INSTRUCTIONS_H
#define NJS_INSTRUCTIONS_H

#include <cstdint>
#include <string>

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;

// has corresponding string representations
enum class OpType {
  neg,

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
  pushi,
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
  prop_assign,
  var_deinit_range,
  var_undef,
  var_dispose,
  var_dispose_range,

  jmp,
  jmp_true,
  jmp_false,
  jmp_cond,

  pop_jmp,
  pop_jmp_true,
  pop_jmp_false,
  pop_jmp_cond,

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

  Instruction(OpType op, u16 opr1, u16 opr2, u16 opr3, u16 opr4);

  Instruction(OpType op, int opr1, int opr2);

  Instruction(OpType op, int opr1);

  explicit Instruction(OpType op);

  Instruction();

  std::string description() const;

  static int get_stack_usage(OpType op_type) {
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
      case OpType::var_deinit_range:
      case OpType::var_undef:
      case OpType::var_dispose:
      case OpType::var_dispose_range:
        return 0;
      case OpType::jmp:
      case OpType::jmp_true:
      case OpType::jmp_false:
      case OpType::jmp_cond:
        return 0;
      case OpType::pop_jmp:
      case OpType::pop_jmp_true:
      case OpType::pop_jmp_false:
      case OpType::pop_jmp_cond:
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
      case OpType::call:    // need special handling
      case OpType::js_new:  // need special handling
      case OpType::ret:
      case OpType::ret_err:
      case OpType::halt:
      case OpType::halt_err:
      case OpType::nop:
        return 0;
    }
  }

  void swap_two_operands();

  OpType op_type;
  union {
    double num_float;
    OperandType1 two;
    OperandType2 four;
  } operand;

};


} // namespace njs

#endif // NJS_INSTRUCTIONS_H