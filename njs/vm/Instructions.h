#ifndef NJS_INSTRUCTIONS_H
#define NJS_INSTRUCTIONS_H

namespace njs {

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
  pop,
  j,
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
  halt,
};

struct Instruction {
  InstType op_type;
  union {
    double num;
    int opr1;
    int opr2;
  } oprand;
  
};


} // namespace njs

#endif // NJS_INSTRUCTIONS_H