#ifndef NJS_GLOBAL_VAR_H
#define NJS_GLOBAL_VAR_H

namespace njs {

class Global {
 public:
  inline static bool show_codegen_result {true};
  inline static bool show_gc_statistics {false};
  inline static bool enable_optimization {false};
  inline static bool show_vm_state {false};
  inline static bool show_vm_exec_steps {false};
  inline static bool show_log_buffer {false};
};

}

#endif