#ifndef NJS_JSRUNLOOP_H
#define NJS_JSRUNLOOP_H

#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "njs/basic_types/JSValue.h"
#include "njs/basic_types/JSFunctionMeta.h"
#include "njs/include/robin_hood.h"
#include "njs/include/BS_thread_pool.hpp"

namespace njs {

class NjsVM;

using robin_hood::unordered_map;

struct JSTask {
  size_t task_id;
  bool use_native_func {false};
  JSValue task_func;
  NativeFuncType native_task_func {nullptr};
  std::vector<JSValue> args;

  bool is_timer;
  // used on linux
  int timer_fd;
  size_t timeout;
  bool repeat {false};

  bool canceled {false};
};

class JSRunLoop {
 public:
  explicit JSRunLoop(NjsVM& vm);
  ~JSRunLoop();

  void loop();

  size_t add_timer(JSFunction* func, size_t timeout, bool repeat);
  bool remove_timer(size_t timer_id);

  JSTask* register_task(JSFunction* func);
  void exec_task(JSTask *task);

  BS::thread_pool& get_thread_pool() { return thread_pool; }

  void gc_gather_roots(std::vector<GCObject **>& roots);

 private:
  void timer_loop();
  void setup_pipe();

  NjsVM& vm;

  size_t task_counter {0};
  // if a macro task has not been completed, it must be in the task pool.
  unordered_map<size_t, JSTask> task_pool;
  // store pointers to the tasks, which are in the task pool.
  std::deque<JSTask *> macro_task_queue;
  std::mutex macro_queue_lock;
  std::condition_variable macro_queue_cv;

  int mux_fd;
  int pipe_write_fd;
  int pipe_read_fd;
  std::thread timer_thread;
  BS::thread_pool thread_pool {2};
};

}


#endif // NJS_JSRUNLOOP_H
