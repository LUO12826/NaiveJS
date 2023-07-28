#ifndef NJS_GLOBAL_VAR_H
#define NJS_GLOBAL_VAR_H

namespace njs {

class Global {
 public:
  inline static bool dump_bytecode {false};
  inline static bool show_gc_statistics {false};
  inline static bool show_vm_end_state {false};
  inline static bool show_vm_exec_steps {true};
};

}

#endif