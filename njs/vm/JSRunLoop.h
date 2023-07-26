#ifndef NJS_JSRUNLOOP_H
#define NJS_JSRUNLOOP_H

#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>

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
  bool canceled {false};
};

class JSRunLoop {
 public:
  explicit JSRunLoop(NjsVM& vm);
  ~JSRunLoop();

  void loop();

  size_t add_timer(JSFunction* func, size_t timeout, bool repeat);
  bool remove_timer(size_t timer_id);

 private:
  void timer_loop();
  void setup_pipe();
  void post_timer_fired_task(JSTask *task);

  NjsVM& vm;

  size_t task_counter {0};
  unordered_map<size_t, JSTask> task_pool;
  std::deque<JSTask> micro_task_queue;
  std::deque<JSTask> macro_task_queue;
  std::mutex marco_queue_lock;
  std::condition_variable marco_queue_cv;

  int kqueue_id;
  int pipe_write_fd;
  int pipe_read_fd;
  std::thread timer_thread;
};

}


#endif // NJS_JSRUNLOOP_H
