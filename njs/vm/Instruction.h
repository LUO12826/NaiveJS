#ifndef NJS_INSTRUCTIONS_H
#define NJS_INSTRUCTIONS_H

#include <cstdint>
#include <string>
#include "njs/common/enums.h"

namespace njs {

using u16 = uint16_t;
using u32 = uint32_t;

// has corresponding string representations
enum class OpType {

#define DEF(opc) opc,
#include "opcode.h"
#undef DEF
  opcode_count,
};

struct Instruction {

  static Instruction num_imm(double num);
  static int get_stack_usage(OpType op_type);

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

  ScopeType get_scope_operand () {
    return static_cast<ScopeType>(operand.two[0]);
  }

  OpType op_type;
  union {
    double num_float;
    int32_t two[2];
  } operand;

};


} // namespace njs

#endif // NJS_INSTRUCTIONS_H