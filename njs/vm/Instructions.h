#ifndef NJS_INSTRUCTIONS_H
#define NJS_INSTRUCTIONS_H

#include <cstdint>
#include <string>

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;

enum class InstType {
  add,
  sub,
  neg,
  mul,
  div,

  logi_and,
  logi_or,
  logi_not,

  push,
  pushi,
  push_str,
  push_atom,
  pop,
  pop_assign,
  store,
  store_assign,

  jmp,
  je,
  jne,

  gt,
  lt,
  ge,
  le,
  ne,
  eq,

  call,
  ret,

  fast_add,
  fast_assign,

  make_func,
  make_obj,
  add_props,
  keypath_visit,

  halt,
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

  std::string description();

  InstType op_type;
  union {
    double num;
    OperandType1 two;
    OperandType2 four;
  } operand;

};


} // namespace njs

#endif // NJS_INSTRUCTIONS_H