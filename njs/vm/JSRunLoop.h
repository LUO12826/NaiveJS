#ifndef NJS_JSRUNLOOP_H
#define NJS_JSRUNLOOP_H

#include <deque>

#include "njs/basic_types/JSValue.h"
#include "njs/include/robin_hood.h"

namespace njs {

class NjsVM;

using robin_hood::unordered_map;

struct JSTask {
  size_t task_id;
  JSValue task_func;
  size_t timeout;
  bool repeat {false};
};

class JSRunLoop {
 public:
  explicit JSRunLoop(NjsVM& vm);

  void wait_for_event();
  size_t add_timer_event(JSFunction* func, size_t timeout, bool repeat);

 private:
  NjsVM& vm;

  int kqueue_id;
  size_t task_counter {0};
  unordered_map<size_t, JSTask> task_pool;
  std::deque<JSTask> micro_task_queue;
  std::deque<JSTask> macro_task_queue;
};

}


#endif // NJS_JSRUNLOOP_H
