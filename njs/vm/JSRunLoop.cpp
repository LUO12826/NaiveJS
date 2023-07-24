#include "JSRunLoop.h"

#include <sys/event.h>
#include <vector>
#include "NjsVM.h"

namespace njs {

JSRunLoop::JSRunLoop(NjsVM& vm): vm(vm) {
  kqueue_id = kqueue();
  if (kqueue_id == -1) {
    perror("kqueue init failed.");
    exit(1);
  }
}

void JSRunLoop::wait_for_event() {
  std::vector<struct kevent> event_slot(20);
  while (true) {
    while (!micro_task_queue.empty()) {
      vm.execute_task(micro_task_queue.front());
      micro_task_queue.pop_front();
    }

    while (!macro_task_queue.empty()) {
      vm.execute_task(macro_task_queue.front());
      macro_task_queue.pop_front();
    }

    if (task_pool.empty() && macro_task_queue.empty()) return;

    if (task_pool.size() > event_slot.size()) {
      event_slot.resize(task_pool.size());
    }

    int ret = kevent(kqueue_id, nullptr, 0, event_slot.data(), task_pool.size(), nullptr);
    if (ret == -1) {
      perror("kqueue poll failed.");
      exit(1);
    }
    for (int i = 0; i < ret; i++) {
      auto *the_task = (JSTask*)event_slot[i].udata;
      vm.execute_task(*the_task);

      if (!the_task->repeat) {
        task_pool.erase(the_task->task_id);
      }
    }
  }

}

size_t JSRunLoop::add_timer_event(JSFunction* func, size_t timeout, bool repeat) {

  JSTask task {
      .task_id = task_counter,
      .task_func = JSValue(func),
      .timeout = timeout,
      .repeat = repeat,
  };

  if (timeout == 0 && !repeat) {
    macro_task_queue.push_back(task);
  }
  else {
    if (task.repeat) assert(task.timeout != 0);

    auto [iter, succeeded] = task_pool.emplace(task.task_id, task);
    JSTask *task_ptr = &iter->second;

    uint16_t event_flags = EV_ADD | EV_ENABLE;
    if (!task.repeat) event_flags |= EV_ONESHOT;

    struct kevent event;
    EV_SET(&event, task.task_id, EVFILT_TIMER, event_flags,
           NOTE_USECONDS, task.timeout * 1000, (void*)task_ptr);

    int ret = kevent(kqueue_id, &event, 1, nullptr, 0, nullptr);
    if (ret == -1) {
      perror("kqueue add task failed.");
      exit(1);
    }
  }

  task_counter += 1;
  return task_counter - 1;
}

}