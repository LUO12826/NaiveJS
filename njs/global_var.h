#ifndef NJS_GLOBAL_VAR_H
#define NJS_GLOBAL_VAR_H

namespace njs {

class Global {
 public:
  static bool dump_bytecode;
  static bool show_gc_statistics;
  static bool show_vm_end_state;
};

}

#endif